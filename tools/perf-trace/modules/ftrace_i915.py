# Copyright (c) 2021 Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

import json

from modules.common import Event, Trace


i915_event_set = set()

FIELD_ARRAY = (1<<8)
FIELD_DYNAMIC = (1<<9)
FIELD_STRING = (1<<10)


class i915Trace(Trace):
    def __init__(self, trace) -> None:
        super().__init__(trace)

    def parse_event(self, raw, fmt) -> Event:
        data = {}
        offset = 0
        for f in fmt:
            flags = f[3]
            if flags == 0:
                val = int.from_bytes(raw[f[1]:f[1]+f[2]], 'little')
                data[f[0]] = val
            elif flags & FIELD_STRING:
                if flags & FIELD_DYNAMIC:
                    val = int.from_bytes(raw[f[1]:f[1]+f[2]], 'little')
                    offset = val & ((1<<16)-1)
                else:
                    offset = f[1]
                string, size = self._parse_data_string(raw[offset:])
                data[f[0]] = string
            elif flags & FIELD_ARRAY:
                if flags & FIELD_DYNAMIC:
                    val = int.from_bytes(raw[f[1]:f[1]+f[2]], 'little')
                    offset = val & ((1<<16)-1)
                    size = val >> 16
                else:
                    offset = f[1]
                    size = f[2]
                array = []
                elemSize = f[3] & 255
                for i in range(0, size//elemSize):
                    elem = raw[offset+i*elemSize:offset+(i+1)*elemSize]
                    array.append(int.from_bytes(elem, 'little'))
                data[f[0]] = array
        # Filter only i915 events
        if 'ctx' in data.keys() and 'seqno' in data.keys() and 'hw_id' in data.keys() and data['hw_id'] > 0:
            key = f"{data['ctx']}_{data['seqno']}_{data['class']}_{data['instance']}"
            phase = "B"
            if key in i915_event_set:
                phase = "E"
            else:
                i915_event_set.add(key)
            device_class = None
            if data['class'] == 0:
                device_class = "Render"
            elif data['class'] == 1:
                device_class = "VEBOX"
            elif data['class'] == 2:
                device_class = "VDBOX"
            thr_name = f"ctx: {data['ctx']}"
            if device_class is not None:
                thr_name += f" ({device_class})"
            return Event(
                name=f"seqno: {data['seqno']}",
                cat="i915",
                ts=0,
                pid=thr_name,
                tid=thr_name,
                ph=phase,
                args=json.dumps(data),
            )
    def _parse_data_string(self, raw):
        str = ''
        size = 0
        while True:
            c = raw[size]
            size += 1
            if c == 0:
                break
            str += chr(c)
        return str, size
