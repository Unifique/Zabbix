#!/usr/bin/env python
# -*- coding: utf-8 -*-

import dns.resolver
import sys
import re

ip_regex = '(([0-9]{1,2}|1[0-9][0-9]|2[0-4][0-9]|25[0-5])\.){3}([0-9]{1,2}|1[0-9][0-9]|2[0-4][0-9]|25[0-5])'
bls = ["zen.spamhaus.org", "spam.abuse.ch", "cbl.abuseat.org", "virbl.dnsbl.bit.nl", "dnsbl.inps.de", 
    "ix.dnsbl.manitu.net", "dnsbl.sorbs.net", "spam.dnsbl.sorbs.net", "bl.spamcannibal.org", "bl.spamcop.net", 
    "xbl.spamhaus.org", "pbl.spamhaus.org", "dnsbl-1.uceprotect.net", "dnsbl-2.uceprotect.net", 
    "dnsbl-3.uceprotect.net", "db.wpbl.info", "barracudacentral.org"]

def ajuda():
        print 'Utilizacao: %s <ip> <rbl>' %(sys.argv[0])
        print 'RBLs:'
        for linha in bls:
                str_linha = linha
                print '%s' % str_linha
        print 'all              (Verificar todas as RBLs)'
if len(sys.argv) != 3:
        ajuda()
        quit()

if len(sys.argv) == 3:
        if re.search(ip_regex, sys.argv[1]) is not None:
                IP = sys.argv[1]
                if sys.argv[2] in bls:
                        use_rbl = sys.argv[2]
                elif sys.argv[2] == 'all':
                        use_rbl = sys.argv[2]
                else:
                        print 'RBL invalida!'
                        ajuda()
                        quit()
        else:
                print '\n### IP invalido! ###\n'
                ajuda()
                quit()
else:
        ajuda()
        quit()
 
for bl in bls:
        try:
                if use_rbl == bl or use_rbl == 'all':
                        try:
                                my_resolver = dns.resolver.Resolver()
                                query = '.'.join(reversed(str(IP).split("."))) + "." + bl
                                answers = my_resolver.query(query, "A")
                                answer_txt = my_resolver.query(query, "TXT")
                                print 'IP: %s IS listed in %s (%s: %s)' %(IP, bl, answers[0], answer_txt[0])
                        except dns.resolver.NXDOMAIN:
                                print 'IP: %s is NOT listed in %s' %(IP, bl)
        except NameError:
                quit()