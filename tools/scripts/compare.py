import re
from operator import itemgetter


def addrace(race, ret):
    print race.groups()
    ret.append((int(race.group('line1')),race.group('var1'),int(race.group('line2')),race.group('var2')))

def parseRacer():
    text = open("races.txt",'r').read()
    ret = []
    reg = r"""Potential race '(?P<id>\d+)' on variable: "(?P<var1>\w*)" and "(?P<var2>\w*)" at:\s+?(?P<file1>[\w\.]*): l.(?P<line1>\d+): (LOAD|STORE)\s+?(?P<file2>[\w\.]*): l.(?P<line2>\d+): (LOAD|STORE)"""
    for race in re.finditer(reg, text):
        addrace(race, ret)
    return ret


def parseRelay():
    text = open("relay.serv",'r').read()
    ret = []
    i = 0
    while True:
        j = 0
        cont = True
        print i
        while cont:
            print "    "+str(j)
            cont = False
            reg = r"""Possible race between access to:\s+?(?P<pref1>(?:\[REP: \d+\])|(?:\w*))\.?(?P<var1>\w*)( @.+?)? and\s+?(?P<pref2>(?:\[REP: \d+\])|(?:\w*))\.?(?P<var2>\w*)( @.+?)?\s[\s\S]*?Accessed at locs:\s+?\[(?:[\w\.]*:\d+:\(\d+\.f\), ){"""+str(i)+r"""}(?P<file1>[\w\.]*):(?P<line1>\d+).+?\] and\s+?\[(?:[\w\.]*:\d+:\(\d+\.f\), ){"""+str(j)+r"""}(?P<file2>[\w\.]*):(?P<line2>\d+).+?\]"""
            #print reg
            for race in re.finditer(reg, text):
                cont = True
                addrace(race, ret)
            j = j + 1    
        if  j == 1 and cont == False:
            break
        i = i + 1        
    return ret

    
def printraces( races, stream ):
    c = 0
    for race in races:
        c = c + 1
        stream.write('{4:4d}: {0:5d} {1:30} {2:5d} {3:30}\n'.format(race[0], race[1], race[2], race[3], c))
    return


mykey = itemgetter(0,2,1,3)

print "parsing racer output"
raceRacer = sorted(parseRacer(), key=mykey)
print "parsing relay output"
raceRelay = sorted(parseRelay(), key=mykey)

outRacer = open("races.racer",'w')
outRelay = open("races.relay",'w')

print "writing races to races.racer"
printraces(raceRacer, outRacer)
print "writing races to races.relay" 
printraces(raceRelay, outRelay)


print "computing difference"
setRacer = set([(x[0],x[2]) for x in raceRacer])
setRelay = set([(x[0],x[2]) for x in raceRelay])
setDiff = setRelay - setRacer
setDiff = setDiff - set((x[1],x[0]) for x in setRacer)
diff = []
for it in setDiff:
    #print #'{0:5d} {1:5d}'.format(it[0],it[1])
    for x in raceRelay:
        if it[0] == x[0] and it[1] == x[2]:
            diff.append(x)
            
print "writing difference to relay-racer.diff"
outDiff = open("relay-racer.diff",'w')
printraces(sorted(diff, key=mykey), outDiff)            
           




print "DONE!"