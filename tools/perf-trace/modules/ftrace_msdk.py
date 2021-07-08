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

class MsdkTrace(Trace):
    def __init__(self, trace) -> None:
        super().__init__(trace)

    def parse_event(self, data) -> Event:
        offset = 19
        task = int.from_bytes(data[offset:offset+2], 'big')
        offset += 2
        opcode = int.from_bytes(data[offset:offset+2], 'big')
        offset += 2
        timestamp = int.from_bytes(data[offset:offset+8], 'little')
        offset += 8
        thread_id = int.from_bytes(data[offset:offset+8], 'little')
        offset += 8
        size = int.from_bytes(data[offset:offset+8], 'little')
        offset += 8
        raw_data = data[offset:]
        return Event(
            name=self.trace.events_mapping[task],
            cat="msdk",
            ts=timestamp,
            pid=thread_id,
            tid=thread_id,
            ph=self.trace.opcode_mapping[opcode],
            args=self._parse_data(task, opcode, raw_data, size),
        )

    def _parse_data(self, task, opcode, raw_data, size):
        if size == 0:
            return "{}"
        index = 0
        if opcode == 1:
            prefix = "in "
        elif opcode == 2:
            prefix = "out "
        else:
            # Ignore other opcodes
            return "{}"
        output = {}
        for data in self.trace.templates_mapping[self.trace.events_desc[task * 4 + opcode]]:
            if data["input_type"] == "win:UInt16":
                output[prefix + data["name"]] = int.from_bytes(raw_data[index:index + 2], byteorder='little')
                index += 2
            elif data["input_type"] == "win:UInt32":
                output[prefix + data["name"]] = int.from_bytes(raw_data[index:index + 4], byteorder='little')
                index += 4
            elif data["input_type"] == "win:Pointer":
                output[prefix + data["name"]] = hex(int.from_bytes(raw_data[index:index + 8], byteorder='little'))
                index += 8
            else:
                print(f'Unknown input type: {data["input_type"]}')
            if data["map_type"] is not None:
                if output[prefix + data["name"]] in self.trace.values_mapping[data["map_type"]]:
                    output[prefix + data["name"]] = (
                        self.trace.strings_mapping[self.trace.values_mapping[data["map_type"]][output[prefix + data["name"]]]])
        return json.dumps(output)
