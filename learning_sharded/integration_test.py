#!/usr/bin/env python3
"""隔离测试：启动临时 Redis 和服务，创建随机测试库并在结束后删除。
需要配置中的 MySQL 账号具备创建、删除测试库的权限，并安装 redis-server。
不要指向生产服务器；测试不修改已有数据库或 Redis 实例。
"""
import configparser
import json
import os
import socket
import struct
import subprocess
import tempfile
import time
import uuid

ROOT = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
os.umask(0o077)
def config(server):
    c = configparser.RawConfigParser(); c.optionxform = str
    c.read(os.path.join(ROOT, 'server', server, 'config.ini'), encoding='utf-8')
    return c

def free_port():
    s = socket.socket(); s.bind(('127.0.0.1', 0)); n = s.getsockname()[1]; s.close(); return n

def wait_port(n, process):
    for _ in range(100):
        if process.poll() is not None: raise RuntimeError('test service exited early')
        try:
            with socket.create_connection(('127.0.0.1', n), timeout=.2): return
        except OSError: time.sleep(.1)
    raise RuntimeError('test port timeout')

def redis(port, password, *args):
    s = socket.create_connection(('127.0.0.1', port), timeout=2); f = s.makefile('rb')
    def call(parts):
        raw = b'*%d\r\n' % len(parts)
        for part in parts:
            b = str(part).encode(); raw += b'$%d\r\n' % len(b) + b + b'\r\n'
        s.sendall(raw); line = f.readline()
        if line.startswith(b'-'): raise RuntimeError('isolated redis command failed')
        if line.startswith(b'$'):
            n = int(line[1:]); return None if n < 0 else (f.read(n), f.read(2))[0].decode()
        return line.decode().strip()
    try:
        call(['AUTH', password]); return call(args)
    finally: f.close(); s.close()

def readn(s, n):
    data = b''
    while len(data) < n:
        part = s.recv(n-len(data))
        if not part: raise RuntimeError('connection closed')
        data += part
    return data

def send(s, message_id, obj):
    data = json.dumps(obj, separators=(',', ':')).encode()
    s.sendall(struct.pack('!HH', message_id, len(data)) + data)

def recv(s, expected):
    message_id, n = struct.unpack('!HH', readn(s, 4)); value = json.loads(readn(s, n))
    if message_id != expected: raise RuntimeError('unexpected message ID')
    return value

def stop(process):
    if process and process.poll() is None:
        process.terminate()
        try: process.wait(timeout=8)
        except subprocess.TimeoutExpired:
            process.kill(); process.wait(timeout=4)
            raise RuntimeError('test service shutdown timed out')

def main():
    cfg1, cfg2 = config('ChatServer'), config('ChatServer2')
    name = 'llfc_cross_test_' + uuid.uuid4().hex[:12]
    if not name.startswith('llfc_cross_test_') or not name.replace('_', '').isalnum():
        raise RuntimeError('invalid test database name')
    env = os.environ.copy(); env['MYSQL_PWD'] = cfg1.get('Mysql', 'Passwd')
    mysql = ['mysql', '--no-defaults', '--protocol=TCP', '--host='+cfg1.get('Mysql','Host'),
             '--port='+cfg1.get('Mysql','Port'), '--user='+cfg1.get('Mysql','User'),
             '--get-server-public-key', '--batch', '--skip-column-names']
    def sql(query):
        p = subprocess.run(mysql, input=query.encode(), stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE, env=env, timeout=15)
        if p.returncode: raise RuntimeError('isolated MySQL operation failed')
        return p.stdout.decode().strip()
    red = None; servers = []; clients = []; made = False
    with tempfile.TemporaryDirectory(prefix='llfc-cross-test-') as td:
        try:
            sql('CREATE DATABASE `' + name + '` CHARACTER SET utf8mb4;'); made = True
            sql('CREATE TABLE `' + name + '`.chat_message ('
                'message_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY, '
                'thread_id BIGINT UNSIGNED NOT NULL, sender_id BIGINT UNSIGNED NOT NULL, '
                'recv_id BIGINT UNSIGNED NOT NULL, content TEXT NOT NULL, '
                'created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, '
                'updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, '
                'status TINYINT NOT NULL DEFAULT 0, '
                'client_msg_id VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin DEFAULT NULL, '
                'UNIQUE KEY uk_sender_client_msg_id(sender_id,client_msg_id)) ENGINE=InnoDB;')
            rp, c1, c2, g1, g2 = (free_port() for _ in range(5))
            if len({rp,c1,c2,g1,g2}) != 5: raise RuntimeError('test port collision')
            secret = 'test-' + uuid.uuid4().hex
            redlog = open(os.path.join(td,'redis.log'),'wb')
            red = subprocess.Popen(['redis-server','--bind','127.0.0.1','--port',str(rp),
                '--save','','--appendonly','no','--requirepass',secret], cwd=td,
                stdout=redlog, stderr=subprocess.STDOUT)
            wait_port(rp,red)
            for uid in (1001,1002,1003):
                info = {'uid':uid,'name':'test','pwd':'','email':'','nick':'test','desc':'','sex':0,'icon':''}
                redis(rp,secret,'SET','utoken_'+str(uid),'test-token-'+str(uid))
                redis(rp,secret,'SET','ubaseinfo_'+str(uid),json.dumps(info))
            configs = [('ChatServer',cfg1,c1,g1,'test-one','test-two',g2),
                       ('ChatServer2',cfg2,c2,g2,'test-two','test-one',g1)]
            for server,cfg,cp,gp,selfname,peername,peerport in configs:
                path = os.path.join(td,server); os.mkdir(path)
                for section,key,value in [('Mysql','Schema',name),('Redis','Host','127.0.0.1'),
                    ('Redis','Port',str(rp)),('Redis','Passwd',secret),('SelfServer','Host','127.0.0.1'),
                    ('SelfServer','Port',str(cp)),('SelfServer','RPCPort',str(gp)),('SelfServer','Name',selfname),
                    ('PeerServer','Servers','isolated_peer')]: cfg.set(section,key,value)
                cfg.add_section('isolated_peer');cfg.set('isolated_peer','Name',peername)
                cfg.set('isolated_peer','Host','127.0.0.1');cfg.set('isolated_peer','Port',str(peerport))
                with open(os.path.join(path,'config.ini'),'w',encoding='utf-8') as f:cfg.write(f)
                log = open(os.path.join(path,'server.log'),'wb')
                sv = subprocess.Popen([os.path.join(ROOT,'build',server,'ChatServer')],cwd=path,
                                       stdout=log,stderr=subprocess.STDOUT)
                servers.append((sv,log));wait_port(cp,sv)
            def login(cp,uid):
                c = socket.create_connection(('127.0.0.1',cp),timeout=5);c.settimeout(10);clients.append(c)
                send(c,1005,{'uid':uid,'token':'test-token-'+str(uid)})
                return c, recv(c,1006)
            a,r = login(c1,1001)
            if r.get('error') != 0: raise RuntimeError('login A failed')
            b,r = login(c2,1002)
            if r.get('error') != 0: raise RuntimeError('login B failed')
            if redis(rp,secret,'GET','uip_1001') != 'test-one' or redis(rp,secret,'GET','uip_1002') != 'test-two':
                raise RuntimeError('routing setup failed')
            req = {'fromuid':1001,'touid':1002,'thread_id':27,'text_array':[{'unique_id':'cross-test-X','content':'hello'}]}
            send(a,1017,req);ack=recv(a,1018);notice=recv(b,1019)
            if ack.get('error') != 0 or not notice.get('chat_datas') or notice['chat_datas'][0]['unique_id'] != 'cross-test-X':
                raise RuntimeError('cross-server delivery failed')
            mid=ack['chat_datas'][0]['message_id'];send(a,1017,req);ack=recv(a,1018)
            if ack.get('error') != 0 or ack['chat_datas'][0]['message_id'] != mid: raise RuntimeError('retry failed')
            b.settimeout(.7)
            try:
                if b.recv(1): raise RuntimeError('duplicate notification')
                raise RuntimeError('receiver disconnected')
            except socket.timeout: pass
            finally: b.settimeout(10)
            if sql('SELECT COUNT(*) FROM `'+name+'`.chat_message;') != '1': raise RuntimeError('duplicate database row')
            print('PASS cross-server chat and idempotent retry')

            anew,r = login(c2,1001)
            if r.get('error') != 0 or recv(a,1021).get('uid') != 1001: raise RuntimeError('cross-server relogin failed')
            new = redis(rp,secret,'GET','usession_1001');a.close();clients.remove(a);time.sleep(.8)
            if not new or redis(rp,secret,'GET','usession_1001') != new or redis(rp,secret,'GET','uip_1001') != 'test-two':
                raise RuntimeError('old cleanup removed new session')
            send(anew,1023,{'fromuid':1001})
            if recv(anew,1024).get('error') != 0: raise RuntimeError('new heartbeat failed')
            print('PASS cross-server relogin and old-session cleanup')

            redis(rp,secret,'ACL','SETUSER','default','-MSET')
            failed,r = login(c1,1003)
            if r.get('error') != 1103 or redis(rp,secret,'GET','uip_1003') is not None or redis(rp,secret,'GET','usession_1003') is not None:
                raise RuntimeError('MSET failure not handled')
            redis(rp,secret,'ACL','SETUSER','default','+MSET')
            send(failed,1005,{'uid':1003,'token':'test-token-1003'})
            if recv(failed,1006).get('error') != 0 or not redis(rp,secret,'GET','usession_1003'):
                raise RuntimeError('login after MSET recovery failed')
            print('PASS injected MSET failure and retry')
            failed.sendall(struct.pack('!HH', 1005, 65535))
            try:
                if failed.recv(1) != b'': raise RuntimeError('invalid frame not closed')
            except ConnectionResetError: pass
            for _ in range(80):
                if (redis(rp,secret,'GET','uip_1003') is None and
                    redis(rp,secret,'GET','usession_1003') is None): break
                time.sleep(.1)
            else: raise RuntimeError('invalid frame session was not cleaned')
            print('PASS invalid length disconnect and online cleanup')
        finally:
            for c in clients: c.close()
            failures=[]
            for p,log in servers:
                try: stop(p)
                except Exception as e: failures.append(str(e))
                log.close()
            try: stop(red)
            except Exception as e: failures.append(str(e))
            if made: sql('DROP DATABASE `'+name+'`;')
            if failures: raise RuntimeError('; '.join(failures))
            print('PASS temporary services and database cleaned up')

if __name__ == '__main__': main()
