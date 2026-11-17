
vscode ➜ .../OC2_labs/3_part/proxy/os-proxy-tests (main) $ **./1-sequential.sh** --port 8080 --url http://xcal1.vodafone.co.uk/5MB.zip --requests 10 --timeout 60
REFERENCE CURL CMD:     curl -o results/result-0 --http0.9 --http1.0 --progress-bar --max-time 60 --location http://xcal1.vodafone.co.uk/5MB.zip
    PROXY CURL CMD:     curl -o results/result-0 --http0.9 --http1.0 --progress-bar --max-time 60 --proxy1.0 localhost:8080 http://xcal1.vodafone.co.uk/5MB.zip

Downloading reference file:
0: 5.33s

Downloading samples through proxy:
1: 4.66s
2: .03s
3: .02s
4: .01s
5: .01s
6: .01s
7: .01s
8: .02s
9: .01s
10: .01s

Checking results (from 1 to 10):
**PASSED**
vscode ➜ .../OC2_labs/3_part/proxy/os-proxy-tests (main) $ **./2-concurrent-batches.sh** --port 8080 --url http://xcal1.vodafone.
co.uk/5MB.zip --requests 50 --timeout 60 --concurrency 20
REFERENCE CURL CMD:     curl -o results/result-0 --http0.9 --http1.0 --progress-bar --max-time 60 --location http://xcal1.vodafone.co.uk/5MB.zip
    PROXY CURL CMD:     curl -o results/result-0 --http0.9 --http1.0 --progress-bar --max-time 60 --proxy1.0 localhost:8080 http://xcal1.vodafone.co.uk/5MB.zip

Downloading reference file:
0: 3.81s

Downloading samples through proxy (parallel connects = 20):
20: 4.82s
15: 4.82s
9: 4.84s
12: 4.83s
19: 4.82s
14: 4.83s
16: 4.83s
13: 4.83s
18: 4.83s
4: 4.84s
10: 4.84s
2: 4.85s
5: 4.85s
1: 4.85s
6: 4.85s
11: 4.84s
3: 4.85s
7: 4.85s
17: 4.83s
8: 4.85s
21: .14s
23: .13s
24: .13s
26: .13s
28: .13s
25: .13s
27: .13s
31: .13s
33: .13s
30: .13s
29: .13s
34: .13s
32: .13s
35: .13s
38: .13s
37: .13s
39: .13s
40: .13s
36: .14s
22: .16s
42: .08s
44: .08s
41: .08s
43: .08s
48: .07s
45: .08s
50: .07s
47: .08s
49: .07s
46: .08s

Checking results (from 1 to 50):
**PASSED**
vscode ➜ .../OC2_labs/3_part/proxy/os-proxy-tests (main) $ **./3-cache-invalidation.sh** --port 8080 --url http://xcal1.vodafone.
co.uk/5MB.zip --requests 10 --timeout 60
REFERENCE CURL CMD:     curl -o results/result-0 --http0.9 --http1.0 --progress-bar --max-time 60 --location http://xcal1.vodafone.co.uk/5MB.zip
    PROXY CURL CMD:     curl -o results/result-0 --http0.9 --http1.0 --progress-bar --max-time 60 --proxy1.0 localhost:8080 http://xcal1.vodafone.co.uk/5MB.zip

Downloading reference file:
0: 5.48s

Downloading samples through proxy:
1: 4.42s
2: .03s
3: .02s
4: .01s
5: .01s
6: .01s
7: .01s
8: .02s
9: .01s
10: .01s

Cache is working

Waiting 5s for cache invalidation

Downloading samples through proxy:
11: 3.70s
12: .04s
13: .02s
14: .02s
15: .01s
16: .02s
17: .02s
18: .02s
19: .02s
20: .02s

Cache invalidation is working

Checking results (from 1 to 20):
**PASSED**
vscode ➜ .../OC2_labs/3_part/proxy/os-proxy-tests (main) $ **./4-parallel-clients.sh** --port 8080 --url http://xcal1.vodafone.co
.uk/5MB.zip --requests 500 --timeout 60
REFERENCE CURL CMD:     curl -o results/result-0 --http0.9 --http1.0 --progress-bar --max-time 60 --location http://xcal1.vodafone.co.uk/5MB.zip
    PROXY CURL CMD:     curl -o results/result-0 --http0.9 --http1.0 --progress-bar --max-time 60 --proxy1.0 localhost:8080 http://xcal1.vodafone.co.uk/5MB.zip

Downloading reference file:
0: 3.98s

Downloading samples through proxy:
275: 4.38s
305: 4.45s
105: 4.56s
362: 4.45s
413: 4.46s
76: 4.58s
199: 4.49s
461: 4.44s
70: 4.59s
220: 4.45s
467: 4.44s
121: 4.58s
493: 4.43s
286: 4.49s
338: 4.50s
363: 4.48s
194: 4.57s
309: 4.49s
446: 4.46s
395: 4.52s
85: 4.59s
419: 4.47s
136: 4.55s
117: 4.59s
390: 4.52s
3: 4.64s
490: 4.45s
125: 4.60s
138: 4.59s
331: 4.52s
377: 4.49s
62: 4.62s
282: 4.50s
397: 4.53s
415: 4.50s
440: 4.48s
265: 4.58s
330: 4.53s
201: 4.53s
313: 4.52s
367: 4.52s
496: 4.46s
455: 4.47s
12: 4.65s
407: 4.53s
191: 4.61s
254: 4.54s
208: 4.58s
287: 4.56s
264: 4.53s
160: 4.56s
431: 4.50s
42: 4.66s
159: 4.62s
360: 4.49s
82: 4.65s
489: 4.48s
414: 4.54s
17: 4.67s
372: 4.51s
447: 4.50s
261: 4.60s
316: 4.48s
52: 4.65s
178: 4.62s
122: 4.62s
186: 4.57s
492: 4.47s
403: 4.48s
283: 4.53s
18: 4.67s
444: 4.50s
134: 4.58s
317: 4.61s
432: 4.51s
233: 4.57s
324: 4.57s
103: 4.65s
115: 4.62s
272: 4.57s
334: 4.57s
228: 4.61s
161: 4.63s
291: 4.60s
284: 4.56s
209: 4.51s
430: 4.51s
6: 4.68s
312: 4.59s
78: 4.66s
1: 4.71s
83: 4.68s
210: 4.76s
402: 4.59s
280: 4.64s
180: 4.61s
24: 4.68s
288: 4.61s
365: 4.57s
91: 4.83s
14: 4.83s
234: 4.76s
441: 4.63s
226: 4.73s
112: 4.75s
424: 4.63s
253: 4.70s
158: 4.74s
336: 4.58s
262: 4.71s
322: 4.68s
246: 4.69s
458: 4.66s
257: 4.64s
45: 4.79s
341: 4.74s
271: 4.61s
212: 4.75s
49: 4.77s
320: 4.70s
144: 4.64s
411: 4.67s
77: 4.71s
357: 4.68s
353: 4.61s
485: 4.65s
97: 4.69s
337: 4.74s
307: 4.63s
26: 4.79s
140: 4.78s
249: 4.73s
399: 4.64s
242: 4.74s
65: 4.72s
437: 4.66s
343: 4.68s
88: 4.72s
197: 4.65s
38: 4.73s
90: 4.78s
335: 4.65s
143: 4.72s
213: 4.74s
230: 4.72s
252: 4.70s
28: 4.83s
266: 4.74s
139: 4.66s
84: 4.82s
184: 4.74s
451: 4.64s
176: 4.66s
15: 4.83s
202: 4.75s
422: 4.64s
302: 4.74s
188: 4.72s
94: 4.82s
434: 4.66s
240: 4.74s
59: 4.69s
183: 4.70s
10: 4.83s
231: 4.74s
491: 4.62s
71: 4.79s
100: 4.71s
304: 4.70s
128: 4.60s
23: 4.79s
20: 4.84s
35: 4.71s
348: 4.65s
326: 4.70s
350: 4.70s
427: 4.62s
163: 4.64s
417: 4.67s
483: 4.59s
135: 4.77s
328: 4.60s
466: 4.55s
205: 4.74s
429: 4.63s
375: 4.70s
151: 4.71s
219: 4.67s
270: 4.76s
132: 4.70s
148: 4.74s
153: 4.72s
473: 4.65s
181: 4.69s
36: 4.83s
245: 4.68s
40: 4.80s
279: 4.64s
179: 4.76s
19: 4.80s
68: 4.70s
25: 4.79s
293: 4.74s
203: 4.72s
389: 4.68s
345: 4.69s
494: 4.65s
239: 4.60s
217: 4.70s
387: 4.67s
124: 4.76s
133: 4.78s
368: 4.63s
267: 4.59s
22: 4.84s
370: 4.67s
72: 4.81s
457: 4.60s
408: 4.65s
58: 4.76s
425: 4.71s
476: 4.58s
168: 4.81s
164: 4.73s
87: 4.78s
204: 4.76s
4: 4.86s
439: 4.62s
423: 4.70s
123: 4.81s
452: 4.67s
296: 4.75s
498: 4.65s
354: 4.57s
141: 4.60s
154: 4.75s
244: 4.76s
167: 4.58s
277: 4.77s
488: 4.61s
392: 4.64s
448: 4.61s
475: 4.57s
462: 4.65s
449: 4.55s
193: 4.74s
435: 4.66s
74: 4.82s
214: 4.71s
412: 4.75s
400: 4.66s
379: 4.66s
107: 4.72s
171: 4.75s
51: 4.85s
381: 4.76s
460: 4.62s
315: 4.64s
263: 4.73s
32: 4.82s
96: 4.84s
298: 4.59s
311: 4.63s
373: 4.68s
268: 4.71s
98: 4.79s
416: 4.70s
95: 4.84s
471: 4.62s
347: 4.61s
21: 4.75s
401: 4.69s
64: 4.85s
495: 4.66s
420: 4.71s
66: 4.83s
478: 4.64s
119: 4.76s
497: 4.68s
155: 4.74s
464: 4.66s
243: 4.71s
376: 4.73s
56: 4.85s
292: 4.82s
410: 4.72s
301: 4.78s
364: 4.74s
327: 4.74s
80: 4.73s
44: 4.70s
150: 4.70s
500: 4.65s
169: 4.69s
166: 4.68s
215: 4.77s
318: 4.61s
346: 4.60s
474: 4.61s
81: 4.83s
371: 4.72s
75: 4.79s
118: 4.78s
238: 4.65s
396: 4.62s
229: 4.80s
386: 4.75s
351: 4.76s
374: 4.72s
127: 4.69s
27: 4.86s
165: 4.68s
391: 4.75s
306: 4.72s
450: 4.70s
48: 4.87s
274: 4.67s
314: 4.76s
398: 4.77s
216: 4.72s
172: 4.77s
241: 4.82s
344: 4.74s
456: 4.69s
256: 4.72s
225: 4.78s
145: 4.83s
295: 4.75s
294: 4.74s
319: 4.78s
147: 4.77s
443: 4.69s
281: 4.72s
192: 4.76s
41: 4.80s
67: 4.85s
472: 4.60s
47: 4.82s
102: 4.83s
388: 4.70s
426: 4.57s
454: 4.57s
468: 4.64s
224: 4.74s
11: 4.77s
113: 4.83s
198: 4.76s
484: 4.68s
106: 4.80s
8: 4.86s
479: 4.70s
130: 4.79s
108: 4.78s
221: 4.78s
187: 4.76s
126: 4.74s
61: 4.86s
218: 4.75s
310: 4.73s
499: 4.59s
482: 4.69s
308: 4.72s
152: 4.83s
111: 4.81s
339: 4.75s
63: 4.75s
285: 4.70s
465: 4.68s
177: 4.81s
31: 4.88s
297: 4.76s
30: 4.77s
189: 4.74s
355: 4.74s
99: 4.78s
299: 4.75s
120: 4.84s
92: 4.72s
250: 4.64s
342: 4.74s
174: 4.76s
101: 4.78s
16: 4.73s
445: 4.61s
463: 4.66s
248: 4.62s
195: 4.72s
258: 4.71s
227: 4.76s
405: 4.75s
481: 4.66s
13: 4.74s
173: 4.58s
114: 4.76s
46: 4.79s
33: 4.87s
93: 4.84s
325: 4.72s
273: 4.72s
329: 4.74s
54: 4.86s
53: 4.83s
109: 4.73s
190: 4.82s
278: 4.68s
247: 4.58s
50: 4.83s
269: 4.72s
332: 4.65s
260: 4.72s
406: 4.65s
470: 4.54s
157: 4.67s
366: 4.74s
356: 4.74s
206: 4.80s
349: 4.71s
453: 4.65s
418: 4.66s
409: 4.69s
142: 4.76s
359: 4.68s
185: 4.65s
37: 4.82s
2: 4.89s
156: 4.74s
255: 4.80s
235: 4.83s
276: 4.75s
384: 4.73s
442: 4.65s
55: 4.87s
438: 4.68s
223: 4.70s
207: 4.74s
251: 4.68s
237: 4.59s
300: 4.76s
104: 4.80s
421: 4.69s
9: 4.89s
358: 4.69s
290: 4.65s
369: 4.75s
137: 4.79s
393: 4.74s
382: 4.59s
394: 4.60s
170: 4.80s
469: 4.69s
383: 4.69s
86: 4.72s
129: 4.78s
7: 4.81s
352: 4.70s
486: 4.66s
236: 4.74s
69: 4.85s
436: 4.55s
73: 4.81s
162: 4.72s
385: 4.72s
477: 4.62s
89: 4.85s
232: 4.78s
361: 4.73s
480: 4.67s
116: 4.75s
39: 4.86s
182: 4.71s
110: 4.71s
404: 4.75s
43: 4.83s
79: 4.85s
259: 4.74s
5: 4.87s
29: 4.86s
57: 4.81s
378: 4.75s
146: 4.76s
196: 4.77s
428: 4.72s
321: 4.76s
131: 4.79s
175: 4.68s
487: 4.70s
149: 4.81s
459: 4.70s
60: 4.83s
380: 4.69s
211: 4.74s
289: 4.67s
303: 4.67s
340: 4.73s
333: 4.74s
200: 4.68s
34: 4.71s
433: 4.70s
323: 4.75s
222: 4.66s

Checking results (from 1 to 500):
**PASSED**
vscode ➜ .../OC2_labs/3_part/proxy/os-proxy-tests (main) $ 

_=**./5-incremental-interrupt.sh**
REFERENCE CURL CMD:     curl -o results/result-0 --http0.9 --http1.0 --progress-bar --max-time 200 --location http://xcal1.vodafone.co.uk/50MB.zip
    PROXY CURL CMD:     curl -o results/result-0 --http0.9 --http1.0 --progress-bar --max-time 200 --proxy1.0 localhost:8080http://xcal1.vodafone.co.uk/50MB.zip

Downloading reference file:
0: 29.31s

Downloading samples through proxy:
1: 
                                                                                                                         0.2%
2: 
##############                                                                                                          12.3%
3: 
##########################################################                                                              49.2%
4: 
###############################################################################                                         67.5%
5: 
######################################################################################################                  87.1%
6: 
################################################################################################################        95.6%
7: 2.07s
8: .15s
9: .12s
10: .12s
11: .12s
12: .12s
13: .12s
14: .12s
15: .12s
16: .12s
17: .12s
18: .15s
19: .12s
20: .13s
21: .13s
22: .12s
23: .13s
24: .13s
25: .13s
26: .13s
27: .13s
28: .15s
29: .12s
30: .12s

Checking results (from 7 to 30):
**PASSED**
vscode ➜ .../OC2_labs/3_part/proxy/os-proxy-tests (main) $ 