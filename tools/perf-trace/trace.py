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

import gzip
import json
import operator
import pathlib
import re
import sys

from xml.dom import minidom

from modules.ftrace_msdk import MsdkTrace
from modules.ftrace_i915 import i915Trace


def read_string(file):
    str = ''
    while True:
        c = file.read(1)
        if c == b'\x00':
            break
        str += c.decode('ascii')
    return str


TYPE_PADDING = 29
TYPE_TIME_EXTEND = 30
TYPE_TIME_STAMP = 31

FIELD_ARRAY = (1<<8)
FIELD_DYNAMIC = (1<<9)
FIELD_STRING = (1<<10)


class TraceTool:
    templates_mapping = {}
    events = []

    def __init__(self) -> None:
        msdk_dir = pathlib.Path(__file__).parent.absolute().parent.parent
        manifest_path = msdk_dir / "_studio" / "shared" / "mfx_trace" / "media_sdk_etw.man"
        xmldoc = minidom.parse(str(manifest_path))
        itemlist = xmldoc.getElementsByTagName("task")
        self.events_mapping = {int(s.attributes['value'].value): s.attributes['name'].value for s in itemlist}
        self.opcode_mapping = {1: "B", 2: "E"}

        templates = xmldoc.getElementsByTagName("template")
        self.events_desc = {
            int(ed.attributes["value"].value): ed.attributes["template"].value
            for ed in xmldoc.getElementsByTagName("event")
            if "template" in ed.attributes.keys()
        }

        self.values_mapping = {}
        for p in xmldoc.getElementsByTagName("valueMap"):
            self.values_mapping[p.attributes["name"].value] = {
                int(ch.attributes["value"].value, base=16 if ch.attributes["value"].value.startswith('0x') else 10):
                ch.attributes["message"].value
                for ch in p.childNodes
                if ch.attributes is not None
            }

        self.strings_mapping = {
            f'$(string.{string.attributes["id"].value})': string.attributes["value"].value
            for string in xmldoc.getElementsByTagName("string")
            if "id" in string.attributes and "value" in string.attributes
        }

        for template in templates:
            data_list = template.getElementsByTagName("data")
            self.templates_mapping[
                template.attributes["tid"].value] = [{
                    "name": data.attributes["name"].value,
                    "input_type": data.attributes["inType"].value,
                    "output_type": data.attributes["outType"].value if "outType" in data.attributes else None,
                    "map_type": data.attributes["map"].value if "map" in data.attributes else None,
                } for data in data_list]

    def trace_file(self, file):
        self.events = []
        self._process_ftrace(file)

    def _process_ftrace(self, file):
        with open(file, "rb") as f:
            # trace-cmd .dat file format.
            # Link: https://man7.org/linux/man-pages/man5/trace-cmd.dat.5.html
            magic = f.read(10)
            if magic != b'\x17\x08\x44\x74\x72\x61\x63\x69\x6e\x67':
                print('invalid ftrace data file')
                return
            ver = read_string(f)
            endian = int.from_bytes(f.read(1), 'little')
            if endian == 1:
                print('big endian is not supported')
                return
            self.pointer_size = int.from_bytes(f.read(1), 'little')
            self.page_size = int.from_bytes(f.read(4), 'little')
            if read_string(f) != 'header_page':
                print('invalid header page in ftrace data file')
                return
            size = int.from_bytes(f.read(8), 'little')
            # skip event header, script don't need that
            f.seek(size, 1)
            # event header
            if read_string(f) != 'header_event':
                print('invalid header event in ftrace data file')
                return
            size = int.from_bytes(f.read(8), 'little')
            # skip event header, script don't need that
            f.seek(size, 1)
            # ftrace event format
            count = int.from_bytes(f.read(4), 'little')
            self.ftrace_event_fmt = {}
            for _ in range(count):
                size = int.from_bytes(f.read(8), 'little')
                self._parse_event_format(f.read(size).decode('ascii'), self.ftrace_event_fmt, 'ftrace')
            # event provider system
            count = int.from_bytes(f.read(4), 'little')
            for _ in range(count):
                pvd = read_string(f)
                num = int.from_bytes(f.read(4), 'little')
                for _ in range(num):
                    size = int.from_bytes(f.read(8), 'little')
                    self._parse_event_format(f.read(size).decode('ascii'), self.ftrace_event_fmt, pvd)
            # ksym info
            size = int.from_bytes(f.read(4), 'little')
            # skip kernel symbol, script don't need that
            f.seek(size, 1)
            # prink info
            size = int.from_bytes(f.read(4), 'little')
            # skip printk symbol, script don't need that
            f.seek(size, 1)
            # process info
            size = int.from_bytes(f.read(8), 'little')
            name = f.read(size).decode('ascii')
            # cpu num info
            cpu_num = int.from_bytes(f.read(4), 'little')
            # option
            str = read_string(f)
            if 'options' in str:
                type = int.from_bytes(f.read(2), 'little')
                while type != 0:
                    size = int.from_bytes(f.read(4), 'little')
                    if size > 0:
                        f.read(size)
                    type = int.from_bytes(f.read(2), 'little')
                str = read_string(f)  # read next option
            # event buffers
            if 'flyrecord' in str:
                self.bufmap = []
                # read buffer in pair of offset and size in file
                for _ in range(cpu_num):
                    offset = int.from_bytes(f.read(8), 'little')
                    size = int.from_bytes(f.read(8), 'little')
                    self.bufmap.append({'offset': offset, 'size': size, 'cur': 0})
            self._parse_events(f)

    def _parse_event_data(self, data):
        event_id = int.from_bytes(data[0:4], 'little') & ((1<<16)-1)
        offset = 8
        if data[offset:offset+4] == b'FTMI':  # msdk trace magic
            return MsdkTrace(self).parse_event(data)
        else:
            return i915Trace(self).parse_event(data, self.ftrace_event_fmt[event_id]['field'])

    def _parse_event_format(self, string, fmt, cid):
        lines = string.splitlines()
        val = re.findall('name:\s+(\w+)', lines[0])
        if len(val) != 1:
            print('invalid event format: ' + string)
            return
        name = val[0]
        val = re.findall('ID:\s+(\d+)', lines[1])
        if len(val) != 1:
            print('invalid event format: ' + string)
            return
        id = int(val[0])
        fields = []
        for i in range(7, len(lines)): # skip common fields in header
            attr = lines[i].split(';')
            # in format of [field: xxx, offset:dd, size:dd, ...]
            if len(attr) > 3:
                offset = int(re.findall(r'\d+', attr[1])[0])
                size = int(re.findall(r'\d+', attr[2])[0])
                fname = attr[0].split(' ')
                flags = 0
                if '[' in attr[0]:
                    s = attr[0].partition('[')
                    s = s[2].partition(']') # cut substr between []
                    if s[0] != '':
                        flags = size//eval(s[0]) # array element size
                    flags |= FIELD_ARRAY
                    if 'char' in attr[0] or 'u8' in attr[0] or 's8' in attr[0]:
                        flags |= FIELD_STRING
                    if '__data_loc' in attr[0]:
                        flags |= FIELD_DYNAMIC
                fields.append([fname[-1], offset, size, flags])
        fmt[id] = {'cid': cid, 'name':name, 'field':fields}

    def _process_page(self, page):
        page_ts = int.from_bytes(page[0:8], 'little')
        offset = 8 + self.pointer_size
        while offset < len(page):
            hdr = int.from_bytes(page[offset:offset+4], 'little')
            offset += 4
            type_len = hdr & ((1 << 5)-1)
            delta = hdr >> 5
            if type_len == TYPE_PADDING:
                offset += int.from_bytes(page[offset:offset+4], 'little')
                size = 0
            elif type_len == TYPE_TIME_EXTEND:
                page_ts += int.from_bytes(page[offset:offset+4], 'little') << 27
                page_ts += delta
                offset += 4
                size = 0
            elif type_len == TYPE_TIME_STAMP:
                page_ts = int.from_bytes(page[offset:offset+4], 'little') << 27
                offset += 4
                size = 0
            elif type_len == 0:
                size = int.from_bytes(page[offset:offset+4], 'little') - 4
                size = (size+3) & ~3
                offset += 4
            else:
                size = type_len * 4
            if size > 0:
                event_data = page[offset:offset+size]
                offset += size
                page_ts += delta
                event = self._parse_event_data(event_data)
                if event is not None:
                    event.ts = page_ts // 1000
                    self.events.append(event)

    def _parse_events(self, f):
        self.events = []
        for buf in self.bufmap:
            while buf['cur'] < buf['size']:
                f.seek(buf['offset'] + buf['cur'], 0)
                data = f.read(self.page_size)
                buf['cur'] += self.page_size
                self._process_page(data)

    def dump_events(self, output_file):
        self.events.sort(key=operator.attrgetter('ts'))
        with (gzip.open(output_file, "wt") if output_file.endswith('.gz') else open(output_file, "w")) as f:
            print("[", file=f)
            for e in self.events:
                # Chrome trace events format.
                # Link: https://docs.google.com/document/d/1CvAClvFfyA5R-PhYUmn5OOQtYMH4h6I0nSsKchNAySU
                print(
                    '{'
                    f'"name": "{e.name}", '
                    f'"cat": "{e.cat}", '
                    f'"ph": "{e.ph}", '
                    f'"pid": "{e.pid}", '
                    f'"tid": "{e.tid}", '
                    f'"ts": {e.ts}, '
                    f'"args": {e.args}'
                    '},', file=f
                )


def main():
    if len(sys.argv) < 3:
        print(f"Usage: python3 {__file__} trace.dat <output>.json")
        sys.exit(1)
    trace_file = sys.argv[1]
    output_file = sys.argv[2]

    trace = TraceTool()
    trace.trace_file(trace_file)
    trace.dump_events(output_file)


if __name__ == "__main__":
    main()
