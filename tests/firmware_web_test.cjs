const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const source = fs.readFileSync('include/web_ui.h', 'utf8');
const helpers = source.slice(source.indexOf('function maintenanceMessage('), source.indexOf('function downloadConfigBackup('));
const install = source.slice(source.indexOf('// v2.0.2: bounded requests'), source.indexOf("$('overview_search')?.addEventListener"));

async function run(mode) {
  const bytes = new Uint8Array(40000); bytes[0] = mode === 'bad_magic' ? 0 : 0xe9;
  const file = new Blob([bytes]); file.name = 'firmware.bin';
  const elements = Object.fromEntries(['firmware_file','firmware_progress','firmware_progress_text','maintenance_status','save','disabled'].map(id => [id,{disabled:id==='disabled',textContent:'',value:0}]));
  elements.firmware_file.files = [file];
  let received = 0, activated = false, aborted = false, reloads = 0, chunks = 0, polls = 0;
  const paths = [], values = [];
  Object.defineProperty(elements.firmware_progress,'value',{get:()=>values.at(-1)||0,set:value=>values.push(value)});
  const response = (data,status=200)=>({ok:status===200,status,json:async()=>data});
  const context = vm.createContext({Blob,Uint8Array,URLSearchParams,AbortController,encodeURIComponent,console,
    $: id=>elements[id], say:()=>{}, confirm:()=>true,
    document:{querySelectorAll:()=>Object.values(elements)},location:{reload:()=>reloads++},
    setTimeout:(fn,ms)=>{if(ms!==30000)queueMicrotask(fn);return 1},clearTimeout:()=>{},
    FormData:class {append(key,blob){this.blob=blob}},
    fetch:async(url,options={})=>{
      const parsed = new URL(url,'http://panel'); paths.push(parsed.pathname);
      if(parsed.pathname==='/api/firmware/begin')return response({ok:true,session:'nonce',boot_id:'before',chunk_bytes:16384});
      if(parsed.pathname==='/api/firmware/chunk'){
        assert.equal(Number(parsed.searchParams.get('offset')),received);
        assert(options.body.blob.size<=16384);
        if(mode==='rejected')return response({ok:false,error:'flash write failed'},400);
        received+=options.body.blob.size;chunks++;
        assert.equal(activated,false,'No activation while chunks are being received');
        if(mode==='lost_chunk'&&chunks===1)throw new TypeError('connection lost after receiving chunk');
        if(mode==='partial'&&chunks===1){received-=100;throw new TypeError('connection lost in chunk')}
        return response({ok:true,active:true,bytes:received});
      }
      if(parsed.pathname==='/api/firmware/status'){
        if(mode==='lost_finish_reboot'&&activated)throw new TypeError('reboot in progress');
        return response({ok:true,active:!activated,verified:activated,bytes:received});
      }
      if(parsed.pathname==='/api/firmware/finish'){
        assert.equal(received,file.size);
        if(mode==='bad_verification')return response({ok:false,error:'firmware validation failed'},400);
        activated=true;
        if(mode.startsWith('lost_finish'))throw new TypeError('reply lost during reboot');
        return response({ok:true,verified:true});
      }
      if(parsed.pathname==='/api/status'){
        polls++;
        return response({version:'2.0.2',boot_id:mode==='no_reboot'||polls===1&&mode!=='lost_finish_reboot'?'before':'after'});
      }
      if(parsed.pathname==='/api/firmware/abort'){aborted=true;return response({ok:true})}
      throw new Error('Unexpected route: '+url);
    }
  });
  vm.runInContext('let maintenanceBusy=false,maintenanceControls=[];'+helpers+install,context);
  await context.installFirmwareUpdate();
  if(['bad_magic','partial','rejected','bad_verification'].includes(mode)) {
    assert(!activated && !reloads);
    assert(!elements.save.disabled && elements.disabled.disabled,'Original control states are restored');
    assert.equal(aborted,mode!=='bad_magic');
    if(mode==='bad_magic')assert.equal(paths.length,0);
    if(mode==='partial'||mode==='rejected')assert(!paths.includes('/api/firmware/finish'));
  } else {
    assert.equal(chunks,3); assert.equal(received,40000); assert(activated);
    assert.equal(reloads,mode==='no_reboot'?0:1);
    if(mode==='no_reboot')assert.match(elements.maintenance_status.textContent,/not been confirmed/);
    assert.deepEqual(values,mode==='lost_finish_reboot'?[0,40,81,100]:[0,40,81,100,100],'Progress follows acknowledged server bytes');
  }
  assert(!elements.maintenance_status.textContent.includes('current firmware remains active'));
  console.log('OTA browser workflow passed: '+mode);
}
(async()=>{
  for(const mode of ['normal','lost_chunk','partial','rejected','bad_verification','lost_finish','lost_finish_reboot','no_reboot','bad_magic'])await run(mode);
  assert(source.includes('async function status(){if(maintenanceBusy)return;'));
  console.log('OTA browser tests passed; background polling is suspended during maintenance.');
})().catch(error=>{console.error(error);process.exitCode=1});
