var o = {a:1, b:2, c:3, d:4};
var s = 0;
for (var i = 0; i < 10000; i++) s = s + o.a + o.b + o.c + o.d;
if (s !== 100000) throw new Error('loop_prop');
