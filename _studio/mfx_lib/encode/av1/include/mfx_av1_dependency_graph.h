// Copyright (c) 2014-2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.


/*

******************************************************************
                             LEGEND
******************************************************************
              +==========+        +----------+
              | GPU TASK |        | CPU TASK |
              +==========+        +----------+


******************************************************************
                             GACC
******************************************************************

                                               -------as ref frame -------
                                               |                         |
                                               v                         |
+----------------+    +==============+    +========+    +========+       |
| INIT NEW FRAME | -> | GPU COPY SRC | -> | GPU ME | -> | GPU MD |       |
+----------------+    +==============+    +========+    +========+       |
        |                                                   |            |
    ---------------------------------------------------------            |
    |                      |                                             |
    v                      v                   +-------------+           |
               ------------------------------> | DEBLOCK ROW |           |
  tile0        |          tile1        |       +-------------+           |
 +-----+    +-----+      +-----+    +-----+            |                 |
 | ENC | -> | ENC |      | ENC | -> | ENC |            |                 |
 +-----+    +-----+      +-----+    +-----+            |                 |
    |          |            |          |               |                 |
 +-----+    +-----+      +-----+    +-----+            |                 |
 | ENC | -> | ENC |      | ENC | -> | ENC |            |                 |
 +-----+    +-----+      +-----+    +-----+            |                 |
               |                       |       +-------------+    +==============+
               ------------------------------> | DEBLOCK ROW | -> | GPU COPY REC |
                          |                    +-------------+    +==============+
                   +-------------+                    |                  |
                   | PACK HEADER |                    |                  |
                   +-------------+                    |                  |
                    /           \                     |                  |
          +-----------+        +-----------+          |                  |
          | PACK TILE |        | PACK TILE |          |                  |
          +-----------+        +-----------+          |                  |
                    \           /                     |                  |
                   +--------------+                   |                  |
                   | ENC_COMPLETE | <------------------                  |
                   +--------------+                                      |
                           |                                             |
                     +----------+                                        |
                     | COMPLETE | <---------------------------------------
                     +----------+


******************************************************************
                             Pure SW
******************************************************************

+----------------+
| INIT NEW FRAME |
+----------------+
        |
    -------------------------------------------- as ref frame -----
    |                      |                                      |
    v                      v                   +-------------+    |
               ------------------------------> | DEBLOCK ROW |    |
  tile0        |          tile1        |       +-------------+    |
 +-----+    +-----+      +-----+    +-----+            |          |
 | ENC | -> | ENC |      | ENC | -> | ENC |            |          |
 +-----+    +-----+      +-----+    +-----+            |          |
    |          |            |          |               |          |
 +-----+    +-----+      +-----+    +-----+            |          |
 | ENC | -> | ENC |      | ENC | -> | ENC |            |          |
 +-----+    +-----+      +-----+    +-----+            |          |
               |                       |       +-------------+    |
               ------------------------------> | DEBLOCK ROW | ----
                          |                    +-------------+
                   +-------------+                    |
                   | PACK HEADER |                    |
                   +-------------+                    |
                   /             \                    |
          +-----------+       +-----------+           |
          | PACK TILE |       | PACK TILE |           |
          +-----------+       +-----------+           |
                    \            /                    |
                   +--------------+                   |
                   | ENC_COMPLETE | <------------------
                   +--------------+
                           |
                     +----------+
                     | COMPLETE |
                     +----------+

*/