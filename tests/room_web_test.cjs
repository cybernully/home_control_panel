const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const source=fs.readFileSync('src/web_manager.cpp','utf8').match(/<script>([\s\S]*?)<\/script>/)[1];
const elements=new Map();let saveHandler;let saved;
function element(id){if(!elements.has(id)) elements.set(id,{value:'',textContent:'',disabled:false,addEventListener(type,cb){saveHandler=cb},querySelector(){return element('save')},set innerHTML(s){this.html=s;for(const m of s.matchAll(/<input id="([^"]+)"[^>]*value="([^"]*)"/g))element(m[1]).value=m[2].replace(/&quot;/g,'"').replace(/&lt;/g,'<').replace(/&gt;/g,'>').replace(/&amp;/g,'&');for(const m of s.matchAll(/<select id="([^"]+)">([\s\S]*?)<\/select>/g)){const v=m[2].match(/value="(\d+)" selected/);element(m[1]).value=v?v[1]:'0'}},get innerHTML(){return this.html}});return elements.get(id)}
const entities=[{entity_id:'light.a',name:'A — light'},{entity_id:'switch.b',name:'<img src=x onerror=alert(1)>'}];
const config={device_id:'test',display_name:'Test',profile:'room',area_id:'office',room_controls:[{entity_id:'light.missing',label:'Missing',placement:1}],media_shortcuts:[]};
const context=vm.createContext({document:{getElementById:element},URLSearchParams,setInterval(){},fetch:async(url,opt)=>({ok:true,json:async()=>{if(url==='/api/room/entities')return entities;if(url==='/api/config'&&!opt)return config;if(url==='/api/config'&&opt){saved=Object.fromEntries(opt.body);return {message:'Saved'}}return {}}})});
// Suppress automatic boot to make network and form roundtrip assertions deterministic.
vm.runInContext(source.slice(0,source.indexOf('status();load().catch')),context);
(async()=>{
 await vm.runInContext('load()',context);
 assert.equal(vm.runInContext('roomRows.length',context),3);
 assert.equal(element('save').disabled,false);
 assert(element('room_fields').innerHTML.includes('&lt;img'));
 element('room_label_1').value='Desk — lamp';element('room_place_1').value='1';
 vm.runInContext('moveRoom(1,-1)',context);
 assert.equal(vm.runInContext('roomRows[0].entity_id',context),'light.a');
 assert.equal(element('room_label_0').value,'Desk — lamp');
 await vm.runInContext('refreshRoom()',context);
 assert.equal(vm.runInContext('roomRows.length',context),3);
 await saveHandler({preventDefault(){}});
 const sent=JSON.parse(saved.room_controls);
 assert.equal(sent[0].label,'Desk — lamp');assert.equal(sent[0].placement,1);
 assert.equal(sent[1].entity_id,'light.missing');
 vm.runInContext('resetRoom(1)',context);
 assert.equal(vm.runInContext('roomRows.length',context),2);
 element('room_place_1').value='2';
 await saveHandler({preventDefault(){}});
 assert.equal(JSON.parse(saved.room_controls)[1].placement,2);
 console.log('Web room editor tests passed: discovery, escaping, reorder, Unicode, missing favorites, hiding, reset and save payload.');
})().catch(e=>{console.error(e);process.exitCode=1});

