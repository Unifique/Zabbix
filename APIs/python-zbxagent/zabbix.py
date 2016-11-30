#!/usr/bin/env python
# -*- coding: UTF-8 -*-
import sys, cgi, os
import httplib, urllib
import ZabbixSender
import logging
import os.path

from ZabbixSender import ZabbixSender

#ip_host = "192.168.66.225"

#Definicoes iniciais do programa
sender = ZabbixSender(u'localhost')
form = cgi.FieldStorage()
client_host = form.getvalue('host')
client_key = form.getvalue('key')
client_valor  = form.getvalue('value')
token = form.getvalue('token')
hosts_file = "/usr/local/etc/zabbix_hosts.conf"
ip_host = cgi.escape(os.environ["REMOTE_ADDR"])
token_info = '*****************' +  str(token)[17:]

# Set up a specific logger with our desired output level
LOG_FILENAME = '/var/log/zabbixsrv/zbxagent.log'
FORMAT = '%(asctime)s [%(levelname)s] (%(clientip)s) --> %(message)s'
d = {'clientip': ip_host}
logging.basicConfig(filename=LOG_FILENAME,filemode='a',format=FORMAT, datefmt='%d/%m/%Y %H:%M:%S',level=logging.INFO)
logger = logging.getLogger('ZBX Agent Client')

# enable debugging
import cgitb
cgitb.enable()

#Checa se os dados do POST nao estao vazios 
def check_post():
        if (not client_host) or (not client_key) or (not client_valor) or (not token):
                return False
        else:
                return True

#Prepara os dados para enviar para o Zabbix
def send_data():
        token_ok=False
        f = open(hosts_file)
        line = f.readline()
        while (line) and (not token_ok):
		ip_token,key_token = line.split("=")
		if (token in key_token) and (ip_host in ip_token):
			logger.info("Token do cliente válido.",extra=d)
		    	token_ok=True
                else:
                        line = f.readline()
        f.close()
        if token_ok:
                send_zabbixtrapper()
                print "Send Zabbix (" + ip_host + ") ----> Host: <" + client_host + ">, key: <" + client_key + ">, value: <" + client_valor + ">"
                print "Dados enviados!"
		logger.info("Dados enviados para o Zabbix.",extra=d)
        else:
		logger.critical("Token do cliente é invalido. Dados recebidos foram descartados.",extra=d)
                print "Token inválido."

#Envia os dados para o servidor da Zabbix
def send_zabbixtrapper():
        sender.AddData(host = u'%s' % (client_host), key = u'%s' % (client_key), value = u'%s' % (client_valor))
        res = sender.Send()
        sender.ClearData()
        print res

#Inicio do programa
logger.info("Conexao estabelecida.",extra=d)
logger.info("Token do cliente: %s; Dados do POST: host:%s | key:%s | value:%s",token_info,client_host,client_key,client_valor,extra=d)
print "Content-Type: text/html;charset=utf-8"
print 
print "<TITLE>Zabbix Sender API</TITLE>"
if check_post():
	logger.info("Dados ok.",extra=d)
	send_data()
else:
	logger.warning("Cliente nao informou os dados, ou estao incompletos.", extra=d)
	print "Não há dados do IP (" + ip_host + ") para enviar para o Zabbix ou estão incompletos."
logger.info("Conexao encerrada.",extra=d)
