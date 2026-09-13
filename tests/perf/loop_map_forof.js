var m = new Map();
for (var i = 0; i < 800; i++) m.set(i, i);
var s = 0;
for (const pair of m) s = s + pair[1];
if (s !== 319600) throw new Error('loop_map_forof');
