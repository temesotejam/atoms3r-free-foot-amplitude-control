'use strict';

// Reader/port cleanup belongs to one session. USB removal only cancels that
// reader; it never nulls a handle still used by another async function.
class UsbSerialMonitor {
  constructor(serial, {data = () => {}, state = () => {}, event = () => {},
      autoReconnect = () => true, diagnosticsEnabled = () => false,
      sleep = ms => new Promise(resolve => setTimeout(resolve, ms))} = {}) {
    Object.assign(this, {serial, data, state, event, autoReconnect, diagnosticsEnabled, sleep});
    this.wanted = false;
    this.task = null;
    this.reader = null;
    this.port = null;
    this.writeTask = Promise.resolve();
    serial?.addEventListener('disconnect', e => {
      if (this.port && (e.target === this.port || e.port === this.port)) {
        this.event('USBデバイスが切断されました');
        this.reader?.cancel().catch(() => {});
      }
    });
  }
  static select(ports, preferred, info) {
    if (ports.includes(preferred)) return {port: preferred};
    if (info.usbVendorId === undefined || info.usbProductId === undefined) return {};
    const matches = ports.filter(port => {
      const other = port.getInfo();
      return other.usbVendorId === info.usbVendorId && other.usbProductId === info.usbProductId;
    });
    return matches.length === 1 ? {port: matches[0]} : {ambiguous: matches.length > 1};
  }
  async connect() {
    if (this.wanted || this.task) return;
    if (!this.serial) { this.state('PC版ChromeまたはEdgeで開いてください', false); return; }
    this.wanted = true;
    this.state('USBポートを選択してください', true);
    try {
      const selected = await this.serial.requestPort(); // User gesture only.
      if (!this.wanted) return;
      this.task = this.run(selected);
      await this.task;
    } catch (error) {
      this.event(`接続を開始できません: ${error.message}`);
    } finally {
      this.wanted = false;
      this.task = null;
      this.state('未接続（ログは保持しています）', false);
    }
  }
  async run(preferred) {
    const info = preferred.getInfo();
    let first = true;
    while (this.wanted) {
      let port = null, reader = null, opened = false;
      const decoder = new TextDecoder();
      try {
        const selected = first ? {port: preferred} : UsbSerialMonitor.select(await this.serial.getPorts(), preferred, info);
        first = false;
        port = selected.port;
        if (selected.ambiguous) {
          this.event('同じ種類のUSB機器が複数あります。「切断」後に対象ポートを選び直してください');
          break;
        }
        if (port && this.wanted) {
          this.state('USBポートを開いています', true);
          await port.open({baudRate: 115200, bufferSize: 65536});
          opened = true;
          if (this.wanted) {
            if (!port.readable) throw new Error('読み取り可能なUSBポートではありません');
            preferred = port;
            this.port = port;
            await this.setDiagnostics(this.diagnosticsEnabled());
            reader = port.readable.getReader();
            this.reader = reader;
            this.event('USB接続・ログ受信開始');
            this.state('接続中 · 115200 bps', true);
            while (this.wanted) {
              const {value, done} = await reader.read();
              if (done) break;
              if (value) this.data(decoder.decode(value, {stream: true}));
            }
          }
        }
      } catch (error) {
        this.event(`USB受信／接続エラー: ${error.message}`);
      } finally {
        const tail = decoder.decode();
        if (tail) this.data(tail);
        if (reader) { try { reader.releaseLock(); } catch (_) {} }
        if (this.reader === reader) this.reader = null;
        if (this.port === port) this.port = null;
        if (opened) { try { await port.close(); } catch (_) {} }
      }
      if (!this.wanted || !this.autoReconnect()) break;
      this.state('USBの復帰待ち · 自動再接続（ログ保持）', true);
      await this.sleep(1000);
    }
  }
  async setDiagnostics(enabled) {
    const port = this.port;
    if (!port?.writable) return;
    const write = async () => {
      if (this.port !== port) return;
      const writer = port.writable.getWriter();
      let timer;
      try {
        await Promise.race([
          writer.write(new TextEncoder().encode(enabled ? 'DIAG ON\n' : 'DIAG OFF\n')),
          new Promise((_, reject) => { timer = setTimeout(() => reject(new Error('USB送信タイムアウト')), 2000); })
        ]);
        this.event(enabled ? 'USB診断ONを要求しました' : 'USB診断OFFを要求しました');
      } catch (error) {
        if (writer.abort) void writer.abort(error).catch(() => {});
        throw error;
      } finally { clearTimeout(timer); writer.releaseLock(); }
    };
    this.writeTask = this.writeTask.then(write, write).catch(error => this.event(`診断切り替え失敗: ${error.message}`));
    await this.writeTask;
  }
  async disconnect() {
    await this.setDiagnostics(false);
    this.wanted = false;
    if (this.reader) { try { await this.reader.cancel(); } catch (_) {} }
    if (this.task) await this.task;
  }
}

if (typeof module !== 'undefined') module.exports = {UsbSerialMonitor};
if (typeof document !== 'undefined') {
  const get = id => document.getElementById(id);
  const log = get('serial-log'), status = get('serial-status');
  const connect = get('serial-connect'), disconnect = get('serial-disconnect');
  let logText = '', renderTimer = null;
  const append = text => {
    logText += text;
    if (logText.length > 2 * 1024 * 1024) logText = '[古いログを省略しました]\n' + logText.slice(-2 * 1024 * 1024);
    // USB arrives in small chunks; do not rebuild a growing DOM on every read.
    if (renderTimer === null) renderTimer = setTimeout(() => {
      log.textContent = logText; log.scrollTop = log.scrollHeight; renderTimer = null;
    }, 250);
  };
  const event = text => append(`\n[PC ${new Date().toISOString()}] ${text}\n`);
  const monitor = new UsbSerialMonitor(navigator.serial, {
    data: append, event,
    autoReconnect: () => get('serial-reconnect').checked,
    diagnosticsEnabled: () => get('serial-diagnostics').checked,
    state: (text, active) => {
      status.textContent = text; connect.disabled = active; disconnect.disabled = !active;
    }
  });
  connect.addEventListener('click', () => { void monitor.connect(); });
  get('serial-diagnostics').addEventListener('change', () => { void monitor.setDiagnostics(get('serial-diagnostics').checked); });
  disconnect.addEventListener('click', () => { void monitor.disconnect(); });
  get('serial-clear').addEventListener('click', () => { logText = ''; log.textContent = ''; });
  get('serial-copy').addEventListener('click', async () => {
    try { await navigator.clipboard.writeText(logText); status.textContent = 'ログをコピーしました'; }
    catch (_) { status.textContent = 'コピーできません。「ログを保存」を使ってください'; }
  });
  get('serial-save').addEventListener('click', () => {
    const url = URL.createObjectURL(new Blob([logText], {type: 'text/plain;charset=utf-8'}));
    const link = document.createElement('a');
    link.href = url; link.download = `atoms3r-usb-${new Date().toISOString().replace(/[:.]/g, '-')}.txt`;
    link.click(); setTimeout(() => URL.revokeObjectURL(url), 1000);
  });
}
