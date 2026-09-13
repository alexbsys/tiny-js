function add(a, b) { return a + b; }
var s = 0;
for (var i = 0; i < 8000; i++) s = add(s, 1);
if (s !== 8000) throw new Error('loop_call');
