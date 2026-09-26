'use strict';
const fs=require('fs'), vm=require('vm'), assert=require('node:assert/strict');
let now=100000, failStart=false, loseReply=false, calls=[];
const storage=new Map(), elements=new Map();
const canvas={clearRect(){},fillRect(){},beginPath(){},moveTo(){},lineTo(){},stroke(){},fillText(){},arc(){},fill(){}};
const element=id=>{if(!elements.has(id))elements.set(id,{textContent:'',style:{},getContext:()=>canvas});return elements.get(id);};
const status={state:'READY_TO_MEASURE',ready:true,running:false,downloadable:false,controller_fresh:true,
  export_phase:'empty',command:{pending:false,submitted:0,completed:0,result:''},foot:{},upright:{}};
const sessionStorage={getItem:k=>storage.get(k),setItem:(k,v)=>storage.set(k,v),removeItem:k=>storage.delete(k)};
const context=vm.createContext({document:{getElementById:element},Date:{now:()=>now},sessionStorage,
  setTimeout:()=>1,clearTimeout(){},AbortController,Map,DataView,Uint8Array,TextDecoder,
  fetch:async(path)=>{
    calls.push(path);
    if(path==='/start-energy-control-autonomous') {
      if(loseReply)throw Error('reply lost after acceptance');
      return failStart ? {ok:false,status:409,text:async()=>'busy'} : {ok:true,text:async()=>'offline_run_queued'};
    }
    assert.equal(path,'/status.json');return {ok:true,json:async()=>status};
  }});
const source=fs.readFileSync('web/pose_comparison.js','utf8')+'\n'+fs.readFileSync('web/runtime.js','utf8').replace(/poll\(\);\s*$/,'');
vm.runInContext(source,context);
(async()=>{
  await vm.runInContext('refresh()',context);assert.equal(element('start').disabled,false);
  calls=[];await vm.runInContext('startOfflineRun()',context);
  assert.deepEqual(calls,['/start-energy-control-autonomous']);
  await vm.runInContext('poll()',context);now+=20000;await vm.runInContext('poll()',context);
  assert.equal(calls.length,1);assert.equal(element('start').disabled,true);
  assert.equal(element('download').disabled,true);assert.equal(element('pitch').textContent,'—');
  assert.ok(storage.size===1);
  // Transport failure after expected finish must keep the UI recoverable.
  now+=26000;
  const fetch=context.fetch;context.fetch=async()=>{throw Error('AP not connected');};
  await vm.runInContext('poll()',context);assert.match(element('connection').textContent,/復帰待ち/);
  context.fetch=fetch;
  Object.assign(status,{state:'ESTOP',ready:false,downloadable:true,last_error:'body_tilt_90deg'});
  await vm.runInContext('poll()',context);
  assert.equal(vm.runInContext('offlineMode',context),false);assert.equal(storage.size,0);
  assert.equal(element('download').disabled,false);assert.match(element('message').textContent,/body_tilt_90deg/);
  Object.assign(status,{state:'READY_TO_MEASURE',ready:true,downloadable:false,last_error:''});
  failStart=true;await vm.runInContext('startOfflineRun()',context);
  assert.equal(vm.runInContext('offlineMode',context),false);
  failStart=false;loseReply=true;calls=[];await vm.runInContext('startOfflineRun()',context);
  await vm.runInContext('poll()',context);assert.equal(calls.length,1);
  assert.equal(vm.runInContext('offlineMode',context),true); // Never retry a possibly accepted START.
  // A reload of an already loaded page preserves the quiet countdown.
  const restored=vm.createContext({...context,sessionStorage,document:{getElementById:element}});
  vm.runInContext(source,restored);assert.equal(vm.runInContext('offlineMode',restored),true);
  console.log('offline browser: no run polling, lost/rejected START, reconnect failure, ESTOP download and restored countdown PASS');
})().catch(e=>{console.error(e);process.exitCode=1;});
