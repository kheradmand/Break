import re

racerin = open("races.txt",'r')
racerout = open("races.racer",'w')

relayin = open("relay.serv",'r')
relayout = open("races.relay",'w')

racer = re.findall("""Potential race '(?P<id>\d+)' on variable: "(?P<var1>\w*)" and "(?P<var2>\w*)" at:\s+?(?P<file1>[\w\.]*): l.(?P<line1>\d+): (LOAD|STORE)\s+?(?P<file2>[\w\.]*): l.(?P<line2>\d+): (LOAD|STORE)""", racein.read())
relay = re.findall("""Possible race between access to:\s+?((?P<pref1>.*?)\.)?(?P<var1>\w*) (@.+?)? and\s+?((?P<pref2>.*?)\.)?(?P<var2>\w*) (@.+?)?\s[\s\S]*?Accessed at locs:\s+?\[(?P<file1>[\w\.]*):(?P<line1>\d+).+?\] and\s+?\[(?P<file2>[\w\.]*):(?P<line2>\d+).+?\]""", relayin.read())
