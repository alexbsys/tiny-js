var s = '';
for (var i = 0; i < 1500; i++) s = s + 'x';
if (s.length !== 1500) throw new Error('loop_string');
