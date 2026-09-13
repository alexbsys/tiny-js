// for (const ... of ...), destructuring in for-of, Map
var passes = 0;
var fails = 0;

function report(area, name, status, detail) {
  print(status + "|" + area + "|" + name + "|" + (detail === undefined ? "" : detail));
  if (status === "PASS") passes = passes + 1;
  else fails = fails + 1;
}
function tryEval(code) {
  try { return { ok: true, value: eval(code), err: "" }; }
  catch (e) { return { ok: false, value: undefined, err: "" + e }; }
}
function expectTrue(area, name, code) {
  var r = tryEval(code);
  if (!r.ok) { report(area, name, "FAIL", "throw: " + r.err); return; }
  if (r.value) report(area, name, "PASS", "true");
  else report(area, name, "FAIL", "got " + r.value);
}

expectTrue("forof", "var_array", "(function(){ var s=0; for (var n of [1,2,3]) s+=n; return s===6; })()");
expectTrue("forof", "const_array", "(function(){ var s=0; for (const n of [1,2,3]) s+=n; return s===6; })()");
expectTrue("forof", "let_array", "(function(){ var s=0; for (let n of [1,2,3]) s+=n; return s===6; })()");
expectTrue("forof", "const_pairs", "(function(){ var s=0; for (const x of [[1,2],[3,4]]) s+=x[0]+x[1]; return s===10; })()");
expectTrue("forof", "const_destructure", "(function(){ var s=0; for (const [a,b] of [[1,2],[3,4]]) s+=a+b; return s===10; })()");
expectTrue("forof", "var_destructure", "(function(){ var s=0; for (var [a,b] of [[1,2],[3,4]]) s+=a+b; return s===10; })()");
expectTrue("forof", "object_rename", "(function(){ var rec={value:9,done:false}; var {value: frame, done: d}=rec; return frame===9 && d===false; })()");

expectTrue("map", "typeof", "typeof Map==='function'");
expectTrue("map", "set_get", "(function(){ var m=new Map(); m.set('a',1); m.set('b',2); return m.get('a')===1 && m.get('b')===2 && m.size===2; })()");
expectTrue("map", "has_delete", "(function(){ var m=new Map(); m.set(1,'x'); return m.has(1) && m.delete(1) && !m.has(1) && m.size===0; })()");
expectTrue("map", "overwrite", "(function(){ var m=new Map(); m.set('k',1); m.set('k',2); return m.get('k')===2 && m.size===1; })()");
expectTrue("map", "forof_pairs", "(function(){ var m=new Map(); m.set('a',1); m.set('b',2); var s=''; for (const [k,v] of m) s+=k+v; return s==='a1b2'; })()");
expectTrue("map", "forof_var_pairs", "(function(){ var m=new Map(); m.set('x',9); var k,v; for (var pair of m) { k=pair[0]; v=pair[1]; } return k==='x' && v===9; })()");
expectTrue("map", "forof_let_pairs", "(function(){ var m=new Map(); m.set('z',3); var s=''; for (let [k,v] of m) s+=k+v; return s==='z3'; })()");
expectTrue("map", "forof_var_destructure", "(function(){ var m=new Map(); m.set('q',4); var s=''; for (var [k,v] of m) s+=k+v; return s==='q4'; })()");
expectTrue("map", "empty_iter", "(function(){ var m=new Map(); var n=0; for (const x of m) n++; return n===0 && m.size===0; })()");
expectTrue("map", "clear", "(function(){ var m=new Map(); m.set(1,2); m.clear(); return m.size===0 && m.get(1)===undefined; })()");
expectTrue("map", "missing_get", "(function(){ var m=new Map(); return m.get('no')===undefined && m.has('no')===false && m.delete('no')===false; })()");
expectTrue("map", "key_strict", "(function(){ var m=new Map(); m.set(1,'n'); m.set('1','s'); return m.get(1)==='n' && m.get('1')==='s' && m.size===2; })()");
expectTrue("map", "object_key", "(function(){ var k={}; var m=new Map(); m.set(k,7); return m.get(k)===7 && m.get({})===undefined; })()");
expectTrue("map", "chained_set", "(function(){ var m=new Map(); m.set('a',1).set('b',2); return m.size===2 && m.get('b')===2; })()");
expectTrue("map", "delete_then_forof", "(function(){ var m=new Map(); m.set('a',1); m.set('b',2); m.delete('a'); var s=''; for (const [k,v] of m) s+=k+v; return s==='b2' && m.size===1; })()");
expectTrue("forof", "object_pattern", "(function(){ var s=0; for (const {a,b} of [{a:1,b:2},{a:3,b:4}]) s+=a+b; return s===10; })()");
expectTrue("forof", "object_rename_loop", "(function(){ var s=0; for (const {value: frame} of [{value:5},{value:7}]) s+=frame; return s===12; })()");
expectTrue("forof", "string_chars", "(function(){ var s=''; for (const ch of 'ab') s+=ch; return s==='ab'; })()");

print("SUMMARY|pass="+passes+" fail="+fails);
result = (fails === 0);
