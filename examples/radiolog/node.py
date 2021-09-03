import paho.mqtt.client as mqtt
import paho.mqtt.publish as publish
import time
import sys
import os
import json
import crccheck.crc


def on_connect(client, userdata, flags, rc):
    print("Connected with result code "+str(rc))
    client.subscribe("radiolog/#")


client = mqtt.Client()
client.username_pw_set("radiolog", "home24radiolog98")
client.on_connect = on_connect

client.connect("fagotto.asterix.cloud", 1317, 60)


cmds = [
    "reset",
    "open",
    "close",
    "stop",
    "go",
    "r",
    "w",
    "dump",
    "sw",
    "log",
    "file",
]


NODEIDs = [
    "Node_85d904",
    "Node_f4a98f",
    "Node_513b66"
]
TOPIC = 0
PAYLOAD = 1

node_list = ["%s: idx=%s" % (n, i) for i, n in enumerate(NODEIDs)]

if len(sys.argv) < 3:
    print("usage %s <nodeid [%s]> <cmd [%s]>" % (sys.argv[0], list(node_list),
                                                 cmds))
    sys.exit(0)

nodeid = sys.argv[1].strip()
key = sys.argv[2].strip()
try:
    cmd_args = sys.argv[3].strip()
except IndexError:
    cmd_args = None

try:
    NODEID = NODEIDs[int(nodeid)]
except IndexError:
    print("wrong ids")
    sys.exit(1)

cmd_table = {
    cmds[0]: ["radiolog/%s/reset" % NODEID, ""],
    cmds[1]: ["radiolog/%s/cover/set" % NODEID, "open"],
    cmds[2]: ["radiolog/%s/cover/set" % NODEID, "close"],
    cmds[3]: ["radiolog/%s/cover/set" % NODEID, "stop"],
    cmds[4]: ["radiolog/%s/cover/set/position" % NODEID, "ARGS"],
    cmds[5]: ["radiolog/%s/cfg/read" % NODEID, "ARGS"],
    cmds[6]: ["radiolog/%s/cfg/write" % NODEID, "ARGS"],
    cmds[7]: ["radiolog/%s/cfg/dump" % NODEID, ""],
    cmds[8]: ["radiolog/%s/switch/set" % NODEID, "ARGS"],
    cmds[9]: [],
    cmds[10]: [],
}


def on_message(client, userdata, msg):
    if NODEID in msg.topic:
        print(msg.topic, " ", str(msg.payload))


if key == "log":
    client.on_message = on_message
    client.loop_forever()
client.loop_start()

if key == 'file':
    client.on_message = on_message
    if cmd_args is None:
        print("You should specify a file to send")
        sys.exit(1)

    with open(cmd_args, 'r') as c:
        topic = None
        for line in c:
            line = line.strip()
            if line.startswith("#"):
                continue
            if "TOPIC" in line:
                topic = line.split("=")[1].strip()
                topic = "radiolog/%s/%s" % (NODEID, topic)
                continue
            if topic is not None:
                client.publish(topic, line)
            time.sleep(1)
    sys.exit(0)


if key in cmd_table:
    print(key)
    cmd = cmd_table[key]
    payload = cmd[PAYLOAD]
    try:
        if payload == "ARGS":
            payload = cmd_args
    except IndexError:
        print("Missing args")
        sys.exit(1)

    client.publish(cmd[TOPIC], payload)
    time.sleep(1)
