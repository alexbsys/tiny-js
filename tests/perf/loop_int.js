var s = 0;
for (var i = 0; i < 20000; i++) s = s + i;
if (s !== 199990000) throw new Error('loop_int');
