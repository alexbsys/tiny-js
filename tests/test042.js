// ArrayBuffer, DataView, Uint8Array, BigInt()
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
function expectEq(area, name, code, want) {
  var r = tryEval(code);
  if (!r.ok) { report(area, name, "FAIL", "throw: " + r.err); return; }
  if (r.value == want) report(area, name, "PASS", "" + r.value);
  else report(area, name, "FAIL", "got " + r.value + " want " + want);
}

expectTrue("ab", "typeof", "typeof ArrayBuffer==='function'");
expectEq("ab", "byteLength", "new ArrayBuffer(8).byteLength", 8);
expectEq("ab", "zero", "new ArrayBuffer(0).byteLength", 0);

expectTrue("u8", "typeof", "typeof Uint8Array==='function'");
expectEq("u8", "length", "new Uint8Array(4).length", 4);
expectEq("u8", "index_zero", "new Uint8Array(3)[1]", 0);
expectEq("u8", "index_set", "(function(){ var a=new Uint8Array(2); a[0]=200; return a[0]; })()", 200);
expectEq("u8", "set_clamps", "(function(){ var a=new Uint8Array(1); a.set([0x1FF],0); return a[0]; })()", 255);
expectEq("u8", "from_array", "new Uint8Array([1,2,3])[2]", 3);
expectTrue("u8", "buffer_link", "(function(){ var a=new Uint8Array(2); return a.buffer && a.buffer.byteLength===2; })()");
expectTrue("u8", "share_buffer", "(function(){ var b=new ArrayBuffer(2); var a=new Uint8Array(b); a[0]=9; var d=new DataView(b); return d.getUint8(0)===9; })()");
expectTrue("u8", "set_method", "(function(){ var a=new Uint8Array(4); a.set([9,8],1); return a[1]===9 && a[2]===8 && a[0]===0; })()");

expectTrue("dv", "typeof", "typeof DataView==='function'");
expectEq("dv", "u8_roundtrip", "(function(){ var d=new DataView(new ArrayBuffer(2)); d.setUint8(1,200); return d.getUint8(1); })()", 200);
expectEq("dv", "i8_neg", "(function(){ var d=new DataView(new ArrayBuffer(1)); d.setInt8(0,-2); return d.getInt8(0); })()", -2);
expectEq("dv", "u16_le", "(function(){ var d=new DataView(new ArrayBuffer(2)); d.setUint16(0,0x0102,true); return d.getUint8(0)===0x02 && d.getUint16(0,true)===0x0102; })()", true);
expectEq("dv", "u16_be", "(function(){ var d=new DataView(new ArrayBuffer(2)); d.setUint16(0,0x0102,false); return d.getUint8(0)===0x01 && d.getUint16(0,false)===0x0102; })()", true);
expectEq("dv", "u32_le", "(function(){ var d=new DataView(new ArrayBuffer(4)); d.setUint32(0,0x01020304,true); return d.getUint8(0)===4 && d.getUint32(0,true)===0x01020304; })()", true);
expectEq("dv", "i32_neg", "(function(){ var d=new DataView(new ArrayBuffer(4)); d.setInt32(0,-1,true); return d.getInt32(0,true); })()", -1);
expectTrue("dv", "view_offset", "(function(){ var b=new ArrayBuffer(4); var d=new DataView(b,2,2); d.setUint16(0,0xAABB,true); return new Uint8Array(b)[2]===0xBB; })()");

expectTrue("bi", "typeof", "typeof BigInt==='function'");
expectEq("bi", "from_int", "BigInt(7)", 7);
expectEq("bi", "from_string", "String(BigInt('9007199254740993'))", "9007199254740993");
expectTrue("bi", "from_n_suffix", "BigInt(1n)==1 || BigInt(1n)===1");
expectTrue("bi", "add_exact", "(function(){ var a=BigInt('9007199254740993'); return String(a+2)==='9007199254740995'; })()");

print("SUMMARY|pass="+passes+" fail="+fails);
result = (fails === 0);
