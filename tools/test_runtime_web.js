'use strict';
const fs = require('fs'), vm = require('vm'), assert = require('assert');
const elements = new Map();
function element(id) {if(!elements.has(id)) elements.set(id, {textContent:'',getContext:()=>new Proxy({}, {get:()=>()=>{}})}); return elements.get(id);}
const code=fs.readFileSync('web/runtime.js','utf8').replace(/poll\(\);\s*$/, '');
const context=vm.createContext({document:{getElementById:element},setTimeout,clearTimeout,AbortController,
  Date,DataView,Uint8Array,TextDecoder,Map,console,fetch:()=>{throw Error('unset');}});
vm.runInContext(code,context);
(async()=>{
  // Headers succeed immediately, response body never completes until aborted.
  context.fetch=async(_path,opts)=>({ok:true,json:()=>new Promise((_,reject)=>opts.signal.addEventListener('abort',()=>reject(Error('body_aborted'))))});
  await assert.rejects(vm.runInContext("request('/status.json',{timeout:25})",context),/body_aborted/);
  assert.strictEqual(vm.runInContext("crc32(new Uint8Array([49,50,51,52,53,54,55,56,57]))",context),0xcbf43926);
  await vm.runInContext('refresh()',context); // real production refresh cleanup
  assert.strictEqual(vm.runInContext('refreshInFlight',context),false);
  context.fetch=async()=>({ok:true,json:async()=>({})});
  assert.deepStrictEqual(JSON.parse(JSON.stringify(await vm.runInContext("request('/status.json')",context))),{});
  const packet=new Uint8Array(19),view=new DataView(packet.buffer);packet.set([1,2,3],16);
  context.packet=packet;view.setUint32(0,0x31484346,true);view.setUint32(4,4096,true);view.setUint32(8,3,true);
  view.setUint32(12,vm.runInContext('crc32(packet.subarray(16))',context),true);
  assert.strictEqual(vm.runInContext('validateChunk(packet,4096,3).length',context),3);
  assert.throws(()=>vm.runInContext('validateChunk(packet,0,3)',context),/不一致/);
  packet[18]^=1;assert.throws(()=>vm.runInContext('validateChunk(packet,4096,3)',context),/CRC/);
  console.log('body timeout, refresh recovery, stale offsets and corrupt chunk rejection PASS');
})().catch(e=>{console.error(e);process.exitCode=1;});
