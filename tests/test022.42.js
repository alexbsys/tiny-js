/* Javascript eval */

mystructure = { a:39, b:3, addStuff : function(c,d) { return c+d; } };

mystring = JSON.stringify(mystructure, undefined);

// 42-tiny-js change begin --->
// in JavaScript eval is not JSON.parse
// use parentheses or JSON.parse instead
//mynewstructure = eval(mystring);
mynewstructure = eval("("+mystring+")");
mynewstructure2 = JSON.parse(mystring);
//<--- 42-tiny-js change end

// ES5 JSON.stringify omits functions. The live object still has addStuff.
result = mynewstructure.a + mynewstructure.b == 42
	&& mynewstructure2.a + mynewstructure2.b == 42
	&& mynewstructure.addStuff === undefined
	&& mynewstructure2.addStuff === undefined
	&& mystructure.addStuff(mystructure.a, mystructure.b) == 42
	&& mystring.indexOf("addStuff") < 0;
