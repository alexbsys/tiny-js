var o = {a:{b:1}};
var s = 0;
for (var i = 0; i < 8000; i++) s = s + (o?.a?.b ?? 0);
if (s !== 8000) throw new Error('loop_opt');
