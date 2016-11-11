import sys
from glob import glob
from itertools import groupby

def parse_table(table):
    sections = []
    for line in table.readlines():
        if line[0] == "#":
            sections.append(["".join(c for c in line if c.isalnum()), []])
            continue
        sections[-1][1] += list(parse_line(line))
    return dict(sections)

def parse_line(line):
    entries = line.split()
    if not entries:
        return
    hours = [int(h) for h in entries[0].split(".") if h]
    if len(hours) > 1:
        hours = range(hours[0], hours[1] + (1 if hours[0] < hours[1] else 25))
    minutes = entries[1:]
    for hour in hours:
        for minute in entries[1:]:
            yield hour, int(minute)


# 1000.0000            add 127 minutes to next
# 1xxx.xxxx nnnn.nnnn  x > 0, repeat x times n minutes
# 0nnn.nnnn            n > 0, n minutes
# 0000.0000            end

def make_array(timetable):
    minutes = [t[0] * 60 + t[1] for t in timetable]
    diffs = [(minutes[i] - minutes[i - 1]) % (24 * 60) for i in range(len(minutes))]
    diffs[0] = minutes[0] - (4 * 60)
    for value, group in groupby(diffs):
        count = sum(1 for _ in group)
        if value > 255 or (count <= 2 and value > 127):
            for i in range(count):
                for j in range(value >> 7):
                    yield 128
                yield value & 127
        elif count <= 2:
            for i in range(count):
                yield value
        else:
            for i in range(count // 127):
                yield 255
                yield value
            yield 128 + (count % 127)
            yield value
    yield 0

HEADER = """
enum tables { %s };

enum sections { %s };

struct meta {
    char *from, *to;
    uint8_t route;
};
"""

def make_cfiles(plan, header):
    tables = sorted(plan.keys(), key = lambda t: plan[t][1][1:])
    sections = set(sum((list(v.keys()) for (v, _) in plan.values()), []))

    header.write(HEADER % (", ".join(tables + ["table_count"]), ", ".join(sections)))
    header.write("const struct meta meta_plan[] = {\n")
    for table in tables:
        [from_, route, to] = plan[table][1]
        header.write("   [%s] = { .from = \"%s\", .to = \"%s\", .route = %s },\n" % (table, from_.upper(), to.upper(), route))
    header.write("};\n\n")

    header.write("const uint8_t *plan[%d][%d] = {\n" % (len(tables), len(sections)))
    for table in tables:
        header.write("    [%s] = (const uint8_t*[]) {\n" % table)
        for section in sections:
            data = ", ".join(str(b) for b in make_array(plan[table][0][section]))
            header.write("       [%s] = (const uint8_t[]) { %s },\n" % (section, data))
        header.write("    },\n")
    header.write("};\n")

if __name__ == "__main__":
    plan = {}
    for table_txt in glob("tables/*.txt"):
        name = "".join(c for c in table_txt[7:-4] if c.isalnum())
        meta = table_txt[7:-4].split("_")
        with open(table_txt) as table:
            plan[name] = (parse_table(table), meta)

    with open("busplan_data.h", "w") as header:
        make_cfiles(plan, header)
