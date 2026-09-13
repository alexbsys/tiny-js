var s = 0;
for (var i = 0; i < 20000; i++) {
  s = s + 1;
  if (s > 200) s = 0;
}
if (s !== 101) throw new Error('loop_small');
