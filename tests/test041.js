// Arrow functions, optional chaining, nullish coalescing, Number.toFixed.
var passes = 0;
var fails = 0;

function report(area, name, status, detail) {
  print(status + "|" + area + "|" + name + "|" + (detail === undefined ? "" : detail));
  if (status === "PASS") passes = passes + 1;
  else fails = fails + 1;
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

function expectThrow(area, name, code) {
  var r = tryEval("(function(){ try { " + code + "; return false; } catch(e) { return true; } })()");
  if (!r.ok) report(area, name, "FAIL", "throw: " + r.err);
  else if (r.value) report(area, name, "PASS", "threw");
  else report(area, name, "FAIL", "no throw");
}

// ---------- arrows ----------
expectEq("arrow", "expr_no_args", "(()=>1)()", 1);
expectEq("arrow", "expr_one_arg", "((x)=>x+1)(4)", 5);
expectEq("arrow", "bare_one_arg", "(function(){ var f=x=>x*2; return f(6); })()", 12);
expectEq("arrow", "two_args", "((a,b)=>a+b)(2,3)", 5);
expectEq("arrow", "block_return", "(function(){ var f=x=>{ var y=x+1; return y*2; }; return f(3); })()", 8);
expectEq("arrow", "block_undefined", "(function(){ var f=()=>{ 1+1; }; return f(); })()", undefined);
expectTrue("arrow", "typeof_function", "typeof (()=>0)==='function'");
expectTrue("arrow", "as_callback", "[1,2,3].map(x=>x+1).join(',')==='2,3,4'");
expectEq("arrow", "as_callback_length", "[1,2,3].map(x=>x+1).length", 3);
expectTrue("arrow", "as_callback_paren", "([1,2,3].map(x=>x+1)).join(',')==='2,3,4'");
expectTrue("arrow", "filter_then_join", "[1,2,3,4].filter(x=>x%2===0).join(',')==='2,4'");
expectTrue("arrow", "nested", "(function(){ var add=a=>b=>a+b; return add(2)(5)===7; })()");
expectTrue("arrow", "closure", "(function(){ var n=10; var f=x=>x+n; n=20; return f(1)===21; })()");
expectTrue("arrow", "lexical_this_method", "(function(){ var o={n:3,f:function(){ return (()=>this.n)(); }}; return o.f()===3; })()");
expectTrue("arrow", "lexical_this_lost_call", "(function(){ var o={n:9,f:function(){ return ()=>this.n; }}; var g=o.f(); return g()===9; })()");
expectTrue("arrow", "lexical_this_nested", "(function(){ var o={n:4,f:function(){ return (()=>()=>this.n)()(); }}; return o.f()===4; })()");
expectTrue("arrow", "in_object_literal", "(function(){ var o={n:5,f:()=>1}; return o.f()===1; })()");
expectTrue("arrow", "parens_group_still_works", "(function(){ return (1+2)===3; })()");
expectTrue("arrow", "comma_not_in_body", "(function(){ var a=0; var f=()=>1; return (f(),2)===2; })()");
expectEq("arrow", "default_unused_zero_arg", "(()=>7)(99)", 7);

// ---------- nullish ?? ----------
expectEq("nullish", "null", "(null??7)", 7);
expectEq("nullish", "undefined", "(undefined??8)", 8);
expectEq("nullish", "zero_kept", "(0??9)", 0);
expectEq("nullish", "false_kept", "(false??9)", false);
expectEq("nullish", "empty_string_kept", "(''??9)", "");
expectEq("nullish", "chain", "(null??undefined??4)", 4);
expectTrue("nullish", "or_then_nullish", "((0||null)??5)===5");
expectTrue("nullish", "and_then_nullish", "((1&&null)??6)===6");
expectTrue("nullish", "ternary_still", "(1?2:3)===2 && (0?2:3)===3");

// ---------- optional chain ?. ----------
expectEq("opt", "present", "({a:{b:1}}).a?.b", 1);
expectEq("opt", "missing_obj", "({a:{b:1}}).z?.b", undefined);
expectEq("opt", "null_base", "(function(){ var o=null; return o?.b; })()", undefined);
expectEq("opt", "undef_base", "(function(){ var o=undefined; return o?.b; })()", undefined);
expectTrue("opt", "short_circuit_deep", "(function(){ var o=null; return o?.foo.bar===undefined; })()");
expectEq("opt", "bracket", "(function(){ var o={a:2}; return o?.['a']; })()", 2);
expectEq("opt", "bracket_missing", "(function(){ var o=null; return o?.['a']; })()", undefined);
expectEq("opt", "call_present", "(function(){ var o={f:function(){return 11;}}; return o.f?.(); })()", 11);
expectEq("opt", "call_missing_fn", "(function(){ var o={}; return o.f?.(); })()", undefined);
expectEq("opt", "call_null_base", "(function(){ var o=null; return o?.f(); })()", undefined);
expectTrue("opt", "call_does_not_eval_args", "(function(){ var n=0; var o=null; o?.f(n=1); return n===0; })()");
expectTrue("opt", "normal_call_still_throws", "(function(){ try { var o=null; o.f(); return false; } catch(e) { return true; } })()");
expectTrue("opt", "ternary_float", "(true?.5:3)===0.5");

// ---------- toFixed ----------
expectEq("toFixed", "int_default", "(1).toFixed()", "1");
expectEq("toFixed", "one_digit", "(1.5).toFixed(1)", "1.5");
expectEq("toFixed", "two_digits", "(1).toFixed(2)", "1.00");
expectEq("toFixed", "zero", "(0).toFixed(2)", "0.00");
expectEq("toFixed", "negative", "(-1.2).toFixed(1)", "-1.2");
expectTrue("toFixed", "typeof", "typeof (1.5).toFixed==='function'");
expectThrow("toFixed", "range", "(1).toFixed(-1)");

print("SUMMARY|pass="+passes+" fail="+fails);
result = (fails === 0);
