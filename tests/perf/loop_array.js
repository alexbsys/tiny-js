var a = [];
for (var i = 0; i < 4000; i++) a.push(i);
var s = 0;
for (var i = 0; i < a.length; i++) s = s + a[i];
if (s !== 7998000) throw new Error('loop_array');
