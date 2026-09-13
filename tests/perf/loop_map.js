var m = new Map();
for (var i = 0; i < 800; i++) m.set(i, i);
var s = 0;
for (var i = 0; i < 800; i++) s = s + m.get(i);
if (s !== 319600) throw new Error('loop_map');
