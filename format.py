lines= []
last= -1
with open('newlogs') as f:
    with open('form', 'w') as d:
        for line in f:
            line = line[:-4]
            line+= '\n'
            tup = line.split()
            cur = int (tup[0])
            if(len(tup) !=3 or int(tup[1]) <=3):
                continue
            assert(last <= cur)
            last = cur
            d.write(line)

