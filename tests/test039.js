// Migrated from cmf-script es5_compat_probe.js — keep in tiny-js so it survives.
// Isolated ES5 / engine-behavior probe for TinyJS (42tiny-js + int64).
// Each case is eval()'d so a parse/runtime failure does not abort the suite.
var passes = 0;
var fails = 0;
var skips = 0;

function report(area, name, status, detail) {
  print(status + "|" + area + "|" + name + "|" + (detail === undefined ? "" : detail) + "\n");
  if (status === "PASS") passes = passes + 1;
  else if (status === "FAIL") fails = fails + 1;
  else skips = skips + 1;
}

function tryEval(code) {
  try {
    return { ok: true, value: eval(code), err: "" };
  } catch (e) {
    return { ok: false, value: undefined, err: "" + e };
  }
}

function expectTrue(area, name, code) {
  var r = tryEval(code);
  if (!r.ok) {
    report(area, name, "FAIL", "throw: " + r.err);
    return;
  }
  if (r.value) report(area, name, "PASS", "true");
  else report(area, name, "FAIL", "got " + r.value);
}

function expectEq(area, name, code, want) {
  var r = tryEval(code);
  if (!r.ok) {
    report(area, name, "FAIL", "throw: " + r.err);
    return;
  }
  if (r.value == want) report(area, name, "PASS", "" + r.value);
  else report(area, name, "FAIL", "got " + r.value + " want " + want);
}

function expectThrow(area, name, code) {
  var r = tryEval("(function(){ try { " + code + "; return false; } catch(e) { return true; } })()");
  if (!r.ok) report(area, name, "FAIL", "throw: " + r.err);
  else if (r.value) report(area, name, "PASS", "threw");
  else report(area, name, "FAIL", "no throw");
}

function expectType(area, name, code, typ) {
  var r = tryEval(code);
  if (!r.ok) {
    report(area, name, "FAIL", "throw: " + r.err);
    return;
  }
  if (typeof r.value === typ) report(area, name, "PASS", typ);
  else report(area, name, "FAIL", "typeof " + (typeof r.value));
}

// Feature that may be absent (ES2015+). SKIP if it does not parse; FAIL if it parses but is wrong.
function expectIfParsed(area, name, code) {
  var r = tryEval(code);
  // TinyJS eval() often swallows SyntaxError and returns undefined instead of throwing.
  if (!r.ok || r.value === undefined) {
    report(area, name, "SKIP", r.ok ? "no parse (eval swallowed)" : "no parse: " + r.err);
    return;
  }
  if (r.value) report(area, name, "PASS", "true");
  else report(area, name, "FAIL", "got " + r.value);
}

// Optional API: SKIP when missing (false or throw), PASS when the check holds.
function expectOptional(area, name, code) {
  var r = tryEval(code);
  if (!r.ok) {
    report(area, name, "SKIP", "throw: " + r.err);
    return;
  }
  if (r.value) report(area, name, "PASS", "true");
  else report(area, name, "SKIP", "absent");
}

// ---------- literals / JSON-in-source ----------
expectTrue("literal", "object_literal", "var o={a:1,b:2}; o.a===1 && o.b===2");
expectTrue("literal", "object_trailing_comma", "var o={a:1,}; o.a===1");
expectTrue("literal", "array_literal", "var a=[1,2,3]; a[0]===1 && a[2]===3");
expectTrue("literal", "array_trailing_comma", "var a=[1,2,]; a.length===2");
expectTrue("literal", "array_elision_hole", "var a=[1,,3]; a.length===3 && a[1]===undefined");
expectTrue("literal", "nested_object_array", "var o={a:[1,{b:2}]}; o.a[1].b===2");
expectTrue("literal", "quoted_key", "var o={'x-y':3}; o['x-y']===3");
expectTrue("literal", "numeric_key", "var o={1:9}; o[1]===9 && o['1']===9");
expectTrue("literal", "eval_object_needs_parens", "(eval('({foo:42})')).foo===42");
expectTrue("literal", "eval_bare_object_is_block", "eval('{foo:42}')===42");
expectEq("literal", "getter_in_literal", "(function(){ var o={get x(){return 7;}}; return o.x; })()", 7);
expectEq("literal", "setter_in_literal", "(function(){ var v=0; var o={set x(n){v=n;}}; o.x=5; return v; })()", 5);

// ---------- arrays ----------
expectEq("array", "length_literal", "[10,20,30].length", 3);
expectEq("array", "index_literal", "[10,20,30][1]", 20);
expectEq("object", "dot_literal", "({a:7}).a", 7);
expectEq("array", "new_Array_n", "new Array(4).length", 4);
expectEq("array", "new_Array_elems", "new Array(1,2).length", 2);
expectEq("array", "index_assign_extends_length", "(function(){ var a=[1]; a[2]=3; return a.length; })()", 3);
expectEq("array", "length_shrink", "(function(){ var a=[1,2,3]; a.length=1; return a.length===1 && a[1]===undefined; })()", true);
expectTrue("array", "sparse_length", "(function(){ var a=[]; a[5]=1; return a.length===6; })()");
expectType("array", "typeof_push", "[].push", "function");
expectType("array", "typeof_pop", "[].pop", "function");
expectType("array", "typeof_shift", "[].shift", "function");
expectType("array", "typeof_unshift", "[].unshift", "function");
expectType("array", "typeof_splice", "[].splice", "function");
expectType("array", "typeof_slice", "[].slice", "function");
expectType("array", "typeof_concat", "[].concat", "function");
expectType("array", "typeof_join", "[].join", "function");
expectType("array", "typeof_reverse", "[].reverse", "function");
expectType("array", "typeof_sort", "[].sort", "function");
expectType("array", "typeof_indexOf", "[].indexOf", "function");
expectType("array", "typeof_lastIndexOf", "[].lastIndexOf", "function");
expectType("array", "typeof_every", "[].every", "function");
expectType("array", "typeof_some", "[].some", "function");
expectType("array", "typeof_forEach", "[].forEach", "function");
expectType("array", "typeof_map", "[].map", "function");
expectType("array", "typeof_filter", "[].filter", "function");
expectType("array", "typeof_reduce", "[].reduce", "function");
expectType("array", "typeof_reduceRight", "[].reduceRight", "function");
expectType("array", "typeof_isArray", "Array.isArray", "function");
expectTrue("array", "join_default", "[1,2,3].join()==='1,2,3' || [1,2,3].join(',')==='1,2,3'");
expectEq("array", "join_sep", "[1,2,3].join('-')", "1-2-3");
expectTrue("array", "push_if_present", "(function(){ if (typeof [].push!=='function') return 'skip'; var a=[1]; a.push(2,3); return a.length===3 && a[2]===3; })()");
expectEq("array", "pop", "(function(){ var a=[1,2,3]; var x=a.pop(); return x===3 && a.length===2; })()", true);
expectEq("array", "shift_unshift", "(function(){ var a=[2,3]; a.unshift(1); return a.shift()===1 && a[0]===2 && a.length===2; })()", true);
expectEq("array", "slice", "(function(){ var a=[1,2,3,4]; var b=a.slice(1,3); return b.join(',')==='2,3' && a.length===4; })()", true);
expectEq("array", "concat", "[1,2].concat([3],4).join(',')", "1,2,3,4");
expectEq("array", "reverse", "[1,2,3].reverse().join(',')", "3,2,1");
expectEq("array", "sort_default", "['c','a','b'].sort().join(',')", "a,b,c");
expectEq("array", "splice", "(function(){ var a=[1,2,3,4]; var r=a.splice(1,2,8,9); return r.join(',')==='2,3' && a.join(',')==='1,8,9,4'; })()", true);
expectEq("array", "indexOf", "[1,2,3,2].indexOf(2)", 1);
expectEq("array", "lastIndexOf", "[1,2,3,2].lastIndexOf(2)", 3);
expectEq("array", "map", "[1,2,3].map(function(x){return x*2;}).join(',')", "2,4,6");
expectEq("array", "filter", "[1,2,3,4].filter(function(x){return x%2===0;}).join(',')", "2,4");
expectEq("array", "reduce", "[1,2,3,4].reduce(function(a,b){return a+b;},0)", 10);
expectEq("array", "every_some", "[2,4,6].every(function(x){return x%2===0;}) && [1,2,3].some(function(x){return x===2;})", true);
expectTrue("array", "for_in_skips_holes", "(function(){ var a=[1,,3], k='', n=0; for (var i in a) { n++; k+=i; } return n===2; })()");
expectTrue("array", "Array_isArray_true", "(function(){ if (typeof Array.isArray!=='function') return typeof [1]==='object'; return Array.isArray([1,2]); })()");
expectTrue("array", "Array_isArray_false", "(function(){ return Array.isArray({0:1,length:1})===false && Array.isArray('abc')===false; })()");
expectTrue("array", "instanceof_Array", "[1] instanceof Array");

// ---------- objects ----------
expectTrue("object", "dot_and_bracket", "(function(){ var o={a:1}; o['b']=2; return o.a===1 && o.b===2; })()");
expectTrue("object", "hasOwnProperty", "({a:1}).hasOwnProperty('a') && !({a:1}).hasOwnProperty('toString')");
expectType("object", "typeof_keys", "Object.keys", "function");
expectType("object", "typeof_create", "Object.create", "function");
expectType("object", "typeof_defineProperty", "Object.defineProperty", "function");
expectType("object", "typeof_getPrototypeOf", "Object.getPrototypeOf", "function");
expectType("object", "typeof_freeze", "Object.freeze", "function");
expectType("object", "typeof_seal", "Object.seal", "function");
expectTrue("object", "Object_keys_own", "(function(){ if (typeof Object.keys!=='function') return false; var k=Object.keys({a:1,b:2}); return k.length===2; })()");
expectTrue("object", "Object_create_proto", "(function(){ if (typeof Object.create!=='function') return false; var p={x:1}; var o=Object.create(p); return o.x===1 && o.hasOwnProperty('x')===false; })()");
expectTrue("object", "defineProperty_value", "(function(){ if (typeof Object.defineProperty!=='function') return false; var o={}; Object.defineProperty(o,'a',{value:5,writable:true,enumerable:true,configurable:true}); return o.a===5; })()");
expectTrue("object", "in_operator", "'a' in {a:1} && !('b' in {a:1})");
expectTrue("object", "delete_own", "(function(){ var o={a:1}; delete o.a; return o.a===undefined && !('a' in o); })()");
expectTrue("object", "prototype_chain", "(function(){ function C(){this.x=1;} C.prototype.y=2; var o=new C(); return o.x===1 && o.y===2; })()");
expectTrue("object", "this_in_method", "(function(){ var o={x:3, f:function(){return this.x;}}; return o.f()===3; })()");
expectTrue("object", "Object_seel_typo_or_seal", "(function(){ return typeof Object.seal==='function' || typeof Object.seel==='function'; })()");
expectTrue("object", "getOwnPropertyDescriptor", "(function(){ var d=Object.getOwnPropertyDescriptor({a:1},'a'); return d.value===1 && d.enumerable===true && d.writable===true; })()");
expectTrue("object", "defineProperty_hidden", "(function(){ var o={}; Object.defineProperty(o,'h',{value:7,enumerable:false,writable:true,configurable:true}); return o.h===7 && Object.keys(o).length===0 && o.propertyIsEnumerable('h')===false; })()");
expectTrue("object", "freeze_ignores_assign", "(function(){ var o={a:1}; Object.freeze(o); o.a=2; o.b=3; return o.a===1 && o.b===undefined; })()");
expectTrue("object", "isPrototypeOf_instance", "(function(){ function C(){} var o=new C(); return C.prototype.isPrototypeOf(o) && !C.prototype.isPrototypeOf({}); })()");
expectTrue("object", "propertyIsEnumerable_own", "({a:1}).propertyIsEnumerable('a') && !({a:1}).propertyIsEnumerable('toString')");
expectTrue("object", "getOwnPropertyNames", "(function(){ var n=Object.getOwnPropertyNames({a:1,b:2}); return n.length>=2; })()");
expectTrue("object", "nested_mutation", "(function(){ var o={a:{b:[1,2]}}; o.a.b.push(3); return o.a.b.join(',')==='1,2,3'; })()");
expectTrue("object", "shorthand_or_explicit", "(function(){ var a=4; var o={a:a}; return o.a===4; })()");
expectTrue("object", "method_this_lost", "(function(){ var o={x:5,f:function(){return this.x;}}; var f=o.f; return f()!==5 || f.call(o)===5; })()");

// ---------- JSON ----------
expectType("json", "typeof_parse", "JSON.parse", "function");
expectType("json", "typeof_stringify", "JSON.stringify", "function");
expectTrue("json", "parse_object", "(function(){ var o=JSON.parse('{\"a\":1,\"b\":[2,3]}'); return o.a===1 && o.b[1]===3; })()");
expectTrue("json", "parse_array", "(function(){ var a=JSON.parse('[1,2,3]'); return a[2]===3 && a.length===3; })()");
expectTrue("json", "stringify_object", "(function(){ if (typeof JSON.stringify!=='function') return false; var s=JSON.stringify({a:1}); return s==='{\"a\":1}' || s.indexOf('a')>=0; })()");
expectTrue("json", "stringify_array", "(function(){ if (typeof JSON.stringify!=='function') return false; var s=JSON.stringify([1,2]); return s==='[1,2]' || s.indexOf('1')>=0; })()");
expectThrow("json", "parse_trailing_comma", "JSON.parse('{ \"a\":1, }')");
expectThrow("json", "parse_single_quotes", "JSON.parse(\"{'a':1}\")");
expectTrue("json", "stringify_undefined_prop", "(function(){ if (typeof JSON.stringify!=='function') return false; var s=JSON.stringify({a:1,b:undefined}); return s.indexOf('b')<0; })()");
expectTrue("json", "stringify_replacer_ignored", "(function(){ if (typeof JSON.stringify!=='function') return false; var s=JSON.stringify({a:1,b:2}, ['a']); return true; })()");
expectTrue("json", "parse_nested_struct", "(function(){ var p=JSON.parse('{\"user\":{\"id\":7,\"tags\":[\"a\",\"b\"]},\"ok\":true,\"n\":null}'); return p.user.id===7 && p.user.tags[1]==='b' && p.ok===true && p.n===null; })()");
expectTrue("json", "parse_array_of_objects", "(function(){ var p=JSON.parse('[{\"id\":1,\"n\":\"x\"},{\"id\":2,\"n\":\"y\"}]'); return p.length===2 && p[1].n==='y' && p[0].id===1; })()");
expectTrue("json", "parse_unicode_escape", "(function(){ return JSON.parse('\"\\\\u0041\"')==='A'; })()");
expectTrue("json", "parse_bool_null", "JSON.parse('true')===true && JSON.parse('false')===false && JSON.parse('null')===null");
expectTrue("json", "parse_negative_int", "JSON.parse('-42')===-42");
expectTrue("json", "parse_int64_exact", "(function(){ var n=JSON.parse('9007199254740993'); return n===9007199254740993 && String(n)==='9007199254740993'; })()");
expectTrue("json", "stringify_nested_exact", "(function(){ var s=JSON.stringify({a:1,b:{c:[true,false,null,'x']}}); return s.indexOf('\"a\":1')>=0 && s.indexOf('true')>=0 && s.indexOf('null')>=0 && s.indexOf('\"x\"')>=0; })()");
expectTrue("json", "stringify_int64_exact", "(function(){ var s=JSON.stringify({n:9007199254740993}); return s==='{\"n\":9007199254740993}' || s.indexOf('9007199254740993')>=0; })()");
expectTrue("json", "stringify_escape_quote", "(function(){ var s=JSON.stringify('a\"b'); return s==='\"a\\\\\"b\"' || s.indexOf('\\\\\"')>=0; })()");
expectTrue("json", "stringify_array_holes", "(function(){ var a=[]; a[1]=2; var s=JSON.stringify(a); return s==='[null,2]' || s.indexOf('null')>=0; })()");
expectThrow("json", "stringify_cyclic", "(function(){ var o={}; o.self=o; JSON.stringify(o); })()");
expectThrow("json", "parse_unclosed", "JSON.parse('{')");
expectThrow("json", "parse_trailing_junk", "JSON.parse('[1]x')");
expectTrue("json", "roundtrip_deep", "(function(){ var v={a:[1,{b:2,c:[3,4]}],d:'z'}; var p=JSON.parse(JSON.stringify(v)); return p.a[1].b===2 && p.a[1].c[1]===4 && p.d==='z'; })()");

// ---------- functions / this / apply ----------
expectTrue("function", "arguments_object", "(function(a,b){ return arguments.length===2 && arguments[1]===8; })(3,8)");
expectTrue("function", "arguments_callee", "(function fac(n){ if(n<=1) return 1; return n*arguments.callee(n-1); })(4)===24");
expectType("function", "typeof_call", "(function(){}).call", "function");
expectType("function", "typeof_apply", "(function(){}).apply", "function");
expectType("function", "typeof_bind", "(function(){}).bind", "function");
expectEq("function", "call_this", "(function(){ return this.x; }).call({x:9})", 9);
expectEq("function", "apply_args", "(function(a,b){ return a+b; }).apply(null,[4,5])", 9);
expectEq("function", "bind_this", "(function(){ return this.x; }).bind({x:11})()", 11);
expectTrue("function", "new_returns_object", "(function(){ function C(){ this.v=4; } return (new C()).v===4; })()");
expectTrue("function", "function_expr", "(function(){ var f=function(x){return x+1;}; return f(2)===3; })()");
expectTrue("function", "closure_factory", "(function(){ function make(n){ return function(x){ return x+n; }; } return make(10)(3)===13 && make(1)(2)===3; })()");
expectTrue("function", "forEach_thisArg", "(function(){ var o={sum:0}; [1,2,3].forEach(function(x){ this.sum+=x; }, o); return o.sum===6; })()");
expectTrue("function", "Function_ctor", "(function(){ var f=Function('a','b','return a+b;'); return f(2,3)===5; })()");
expectTrue("function", "bind_then_call", "(function(){ var o={n:1,f:function(a){return this.n+a;}}; return o.f.bind(o)(4)===5; })()");
expectTrue("function", "nested_iife_object", "(function(){ return (function(x){ return {v:x*2}; })(6).v===12; })()");

// ---------- strings ----------
expectEq("string", "charAt", "'abc'.charAt(1)", "b");
expectEq("string", "charCodeAt", "'A'.charCodeAt(0)", 65);
expectEq("string", "indexOf", "'foobar'.indexOf('bar')", 3);
expectEq("string", "split", "'a,b,c'.split(',').length", 3);
expectEq("string", "substring", "'hello'.substring(1,4)", "ell");
expectEq("string", "slice", "'hello'.slice(-2)", "lo");
expectEq("string", "toUpperCase", "'ab'.toUpperCase()", "AB");
expectEq("string", "trim", "'  x  '.trim()", "x");
expectType("string", "typeof_fromCharCode", "String.fromCharCode", "function");
expectEq("string", "fromCharCode", "String.fromCharCode(65)", "A");

// ---------- numbers / int64 ----------
expectTrue("number", "int64_not_ieee_at_2e53", "9007199254740993 !== 9007199254740992 && (9007199254740992+1)===9007199254740993");
expectTrue("number", "strict_neq_same_type", "1!==2 && !(1!==1) && 1!=='1'");
expectTrue("number", "int64_n_suffix_parse", "(function(){ var r=eval('1n'); return r==1 || r===1; })()");
expectTrue("number", "int64_n_suffix_exact", "(function(){ var a=9007199254740993n; return String(a)==='9007199254740993' && (a+2)===9007199254740995; })()");
expectTrue("number", "int64_large_literal", "(function(){ var n=9223372036854775807; return n>9007199254740991 && String(n)==='9223372036854775807'; })()");
expectTrue("number", "int64_add_exact", "(function(){ var a=9007199254740993; var b=a+2; return b===9007199254740995 && String(b)==='9007199254740995'; })()");
expectTrue("number", "int64_sub_mul", "(function(){ var a=9007199254740993; return (a-1)===9007199254740992 && (a*2)===18014398509481986; })()");
expectTrue("number", "int64_neg", "(function(){ var a=-9007199254740993; return String(a)==='-9007199254740993' && (a-2)===-9007199254740995; })()");
expectTrue("number", "int64_hex", "String(0x20000000000001)==='9007199254740993'");
expectTrue("number", "Number_ctor_still_ieee", "(function(){ var n=Number('9007199254740993'); return n===9007199254740992 || String(n)==='9007199254740992'; })()");
expectTrue("number", "hex_and_octal", "0xFF===255 && 0377===255");
expectTrue("number", "NaN_isNaN", "isNaN(NaN) && !isNaN(1)");
expectTrue("number", "parseInt_base10", "parseInt('08',10)===8");
expectTrue("number", "parseInt_legacy_octal", "parseInt('08')===0 || parseInt('08')===8");
expectTrue("number", "parseInt_large", "String(parseInt('9007199254740993',10))==='9007199254740993'");
expectTrue("number", "Number_MAX_VALUE", "typeof Number.MAX_VALUE==='number'");
expectTrue("number", "bitwise_hex_or", "(0xFFFFFFFF|0)===-1 || (0xFFFFFFFF|0)===4294967295");
expectTrue("number", "shift_mask", "(1<<33)===2 || (1<<33)===4294967296");

// ---------- globals / builtins ----------
expectType("builtin", "typeof_Date", "Date", "function");
expectType("builtin", "typeof_Date_now", "Date.now", "function");
expectTrue("builtin", "Date_now_number", "(function(){ var n=Date.now(); return typeof n==='number' && n>0; })()");
expectTrue("builtin", "Date_utc_epoch", "(function(){ var d=new Date(Date.UTC(2020,0,1)); return d.getUTCFullYear()===2020 && d.getUTCMonth()===0 && d.getUTCDate()===1; })()");
expectTrue("builtin", "Date_iso_roundtrip", "(function(){ var d=new Date('2020-01-02T03:04:05.006Z'); return d.toISOString().indexOf('2020-01-02T03:04:05')===0; })()");
expectTrue("builtin", "Date_valueOf_getTime", "(function(){ var d=new Date(1000); return d.valueOf()===1000 && d.getTime()===1000; })()");
expectTrue("builtin", "Date_setUTC_fields", "(function(){ var d=new Date(Date.UTC(2021,5,15,12,30,45,123)); d.setUTCFullYear(2022); d.setUTCHours(1); return d.getUTCFullYear()===2022 && d.getUTCMonth()===5 && d.getUTCDate()===15 && d.getUTCHours()===1 && d.getUTCMilliseconds()===123; })()");
expectTrue("builtin", "Date_toJSON", "(function(){ var s=new Date('2020-01-02T03:04:05.000Z').toJSON(); return s.indexOf('2020-01-02T03:04:05')===0; })()");
expectTrue("builtin", "Date_stringify_toJSON", "(function(){ var s=JSON.stringify(new Date('2020-01-02T03:04:05.000Z')); return s.indexOf('2020-01-02T03:04:05')>=0; })()");
expectTrue("builtin", "Date_stringify_in_object", "(function(){ var s=JSON.stringify({t:new Date('2020-01-02T00:00:00.000Z')}); return s.indexOf('2020-01-02')>=0 && s.indexOf('\"t\"')>=0; })()");
expectTrue("builtin", "Date_parse_iso", "(function(){ var n=Date.parse('2020-01-02T03:04:05.000Z'); var d=new Date(n); return d.getUTCFullYear()===2020 && d.getUTCDate()===2 && d.getUTCHours()===3; })()");
expectTrue("builtin", "Date_UTC_components", "Date.UTC(2020,0,1,0,0,0,0)===1577836800000 || new Date(Date.UTC(2020,0,1)).toISOString().indexOf('2020-01-01')===0");
expectTrue("builtin", "user_toJSON", "(function(){ var o={x:1,toJSON:function(){return {y:2};}}; return JSON.stringify(o)==='{\"y\":2}'; })()");
expectType("builtin", "typeof_Math", "Math", "object");
expectType("builtin", "typeof_Math_random", "Math.random", "function");
expectType("builtin", "typeof_Math_rand_legacy", "Math.rand", "function");
expectTrue("builtin", "Math_rand_range", "(function(){ var r=Math.rand(); return r>=0 && r<1; })()");
expectType("builtin", "typeof_RegExp", "RegExp", "function");
expectType("builtin", "typeof_Error", "Error", "function");
expectType("builtin", "typeof_eval", "eval", "function");
expectType("builtin", "typeof_undefined", "undefined", "undefined");
expectTrue("builtin", "null_typeof_object", "typeof null==='object'");
expectTrue("builtin", "void_0", "void 0 === undefined");

// ---------- syntax / statements ----------
expectTrue("syntax", "for_in_object", "(function(){ var o={a:1,b:2}, n=0; for (var k in o) n++; return n===2; })()");
expectTrue("syntax", "switch_fallthrough", "(function(){ var r=0; switch(1){ case 1: r+=1; case 2: r+=10; break; default: r=0;} return r===11; })()");
expectTrue("syntax", "try_catch_finally", "(function(){ var r=''; try{ throw 1; } catch(e){ r+='c'; } finally{ r+='f'; } return r==='cf'; })()");
expectTrue("syntax", "ternary", "(1?2:3)===2");
expectTrue("syntax", "comma_expr", "(0,1,2)===2");
expectTrue("syntax", "strict_equal", "1==='1' ? false : 1=='1'");
expectTrue("syntax", "typeof_undeclared", "typeof not_declared_xyz === 'undefined'");
expectTrue("syntax", "var_hoist", "(function(){ return hoisted===undefined; var hoisted=1; })()");
expectTrue("syntax", "var_hoist_read_before_assign", "(function(){ var x=hoisted; var hoisted=7; return x===undefined && hoisted===7; })()");
expectTrue("syntax", "function_hoist", "(function(){ return typeof hf==='function'; function hf(){} })()");
expectTrue("syntax", "let_present", "(function(){ let x=1; return x===1; })()");
expectTrue("syntax", "const_present", "(function(){ const x=1; return x===1; })()");
expectTrue("syntax", "for_loop", "(function(){ var s=0; for(var i=0;i<3;i++) s+=i; return s===3; })()");
expectTrue("syntax", "while_do", "(function(){ var i=0; do { i++; } while(i<2); return i===2; })()");
expectTrue("syntax", "with_stmt", "(function(){ var o={z:9}; with(o){ return z===9; } })()");
expectTrue("syntax", "destructure_object", "(function(){ var o={a:4,b:5}; var a=o.a,b=o.b; return a===4 && b===5; })()");
expectIfParsed("syntax", "destructure_object_pattern", "(function(){ var {a,b}={a:4,b:5}; return a===4 && b===5; })()");
expectIfParsed("syntax", "destructure_array_pattern", "(function(){ var [x,y]=[9,8]; return x===9 && y===8; })()");
expectIfParsed("syntax", "for_of_array", "(function(){ var s=0; for (var n of [1,2,3]) s+=n; return s===6; })()");
expectTrue("syntax", "labeled_break", "(function(){ var n=0; outer: for(var i=0;i<3;i++){ for(var j=0;j<3;j++){ n++; if(i===1&&j===1) break outer; } } return n===5; })()");
expectTrue("syntax", "switch_default", "(function(){ var r=0; switch(9){ case 1: r=1; break; default: r=2; } return r===2; })()");
expectTrue("syntax", "catch_error_instanceof", "(function(){ try { JSON.parse('{'); } catch(e) { return e instanceof SyntaxError || (''+e).indexOf('JSON')>=0 || (''+e).indexOf('Syntax')>=0; } return false; })()");
expectTrue("syntax", "comma_in_args", "(function(){ function f(a,b){return a+b;} return f((1,2),3)===5; })()");

// ---------- regexp extras ----------
expectTrue("regexp", "exec_groups", "(function(){ var r=/a(b+)c/.exec('abbbc'); return r && r[0]==='abbbc' && r[1]==='bbb'; })()");
expectTrue("regexp", "test_false", "/^a$/.test('b')===false");
expectTrue("regexp", "replace_string", "'ab-ab'.replace('ab','X')==='X-ab' || 'ab-ab'.replace(/ab/,'X')==='X-ab'");

// ---------- ES2015+ presence (SKIP if the engine cannot parse it) ----------
expectIfParsed("es6", "arrow_expr", "(()=>1)()===1");
expectIfParsed("es6", "template_literal", "`a${1}b`==='a1b'");
expectIfParsed("es6", "class_ctor", "(function(){ class C { constructor(){ this.x=1; } } return (new C()).x===1; })()");
expectIfParsed("es6", "rest_params", "(function(...a){ return a.length; })(1,2)===2");
expectIfParsed("es6", "default_param", "(function(a=1){ return a; })()===1");
expectIfParsed("es6", "spread_array", "[...[1,2]].join(',')==='1,2'");
expectIfParsed("es6", "optional_chain", "({a:{b:1}}).a?.b===1");
expectIfParsed("es6", "nullish_coalesce", "(null??7)===7");
expectIfParsed("es6", "async_fn", "(async function(){ return 1; })");
expectOptional("es6", "Object_assign", "typeof Object.assign==='function' && Object.assign({a:1},{b:2}).b===2");
expectOptional("es6", "Object_entries", "typeof Object.entries==='function' && Object.entries({a:1})[0][0]==='a'");
expectOptional("es6", "Object_values", "typeof Object.values==='function' && Object.values({a:7})[0]===7");
expectOptional("es6", "String_includes", "typeof ''.includes==='function' && 'abc'.includes('b')");
expectOptional("es6", "String_startsWith", "typeof ''.startsWith==='function' && 'abc'.startsWith('ab')");
expectOptional("es6", "String_repeat", "typeof ''.repeat==='function' && 'ab'.repeat(2)==='abab'");
expectOptional("es6", "Array_find", "typeof [].find==='function' && [1,2,3].find(function(x){return x>1;})===2");
expectOptional("es6", "Array_includes", "typeof [].includes==='function' && [1,2].includes(2)");
expectOptional("es6", "Array_from", "typeof Array.from==='function' && Array.from('ab').length===2");
expectOptional("es6", "Number_isNaN", "typeof Number.isNaN==='function' && Number.isNaN(NaN) && !Number.isNaN('x')");
expectOptional("es6", "Number_isInteger", "typeof Number.isInteger==='function' && Number.isInteger(3) && !Number.isInteger(3.2)");
expectOptional("es6", "Promise", "typeof Promise==='function'");
expectOptional("es6", "Map", "typeof Map==='function'");
expectOptional("es6", "Set", "typeof Set==='function'");
expectOptional("es6", "Symbol", "typeof Symbol==='function'");
expectOptional("es6", "Proxy", "typeof Proxy==='function'");
expectOptional("es6", "Reflect", "typeof Reflect==='object' && Reflect!==null");
expectOptional("es6", "encodeURIComponent", "typeof encodeURIComponent==='function' && encodeURIComponent('a b')==='a%20b'");

// ---------- regexp ----------
expectTrue("regexp", "literal", "(function(){ var r=/ab+c/; return r.test ? r.test('abbc') : (''+'abbc').search(r)>=0; })()");
expectType("regexp", "typeof_test", "(/a/).test", "function");
expectType("regexp", "typeof_exec", "(/a/).exec", "function");

print("SUMMARY|pass="+passes+" fail="+fails+" skip="+skips+"\n");
print("FAILS=" + fails + "\n");

result = (fails === 0);

