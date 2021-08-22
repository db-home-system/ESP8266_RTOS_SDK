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
    "open" ,
    "close",
    "go"   ,
    "r"    ,
    "w"    ,
    "dump"
]


NODEIDs = [
        "Node_85d904",
        "Node_f4a98f",
        "Node_513b66"
        ]
TOPIC=0
PAYLOAD=1

node_list = ["%s: idx=%s" % (n,i) for i,n in enumerate(NODEIDs)]

if len(sys.argv) < 3:
    print("usage %s nodeid <%s> cmd <%s>" % (sys.argv[0], list(node_list),
        cmds))
    sys.exit(0)

try:
    nodeid = sys.argv[1].strip()
    key = sys.argv[2].strip()
    cmd_args = sys.argv[3].strip()
except IndexError:
    pass

try:
    NODEID = NODEIDs[int(nodeid)]
except IndexError:
    print("wrong ids")
    sys.exit(1)

cmd_table = {
   cmds[0] : ["radiolog/%s/reset" % NODEID, ""],
   cmds[1] : ["radiolog/%s/cover/set" % NODEID, "open"],
   cmds[2] : ["radiolog/%s/cover/set" % NODEID, "close"],
   cmds[3] : ["radiolog/%s/cover/set_position" % NODEID, "ARGS"],
   cmds[4] : ["radiolog/%s/cfg/read" % NODEID, "ARGS"],
   cmds[5] : ["radiolog/%s/cfg/write" % NODEID, "ARGS"],
   cmds[6] : ["radiolog/%s/cfg/dump" % NODEID, ""],
}

cmd_table_cfg = [
    ["radiolog/%s/cfg/write" % NODEID , "cover_open:0"] ,
    ["radiolog/%s/cfg/read" % NODEID  , "cover_open"] ,

    ["radiolog/%s/cfg/write" % NODEID , "cover_close:1"] ,
    ["radiolog/%s/cfg/read" % NODEID  , "cover_close"] ,

    ["radiolog/%s/cfg/write" % NODEID , "cover_up_time:25"] ,
    ["radiolog/%s/cfg/read" % NODEID  , "cover_up_time"] ,

    ["radiolog/%s/cfg/write" % NODEID , "cover_down_time:25"] ,
    ["radiolog/%s/cfg/read" % NODEID  , "cover_down_time"] ,

    ["radiolog/%s/cfg/write" % NODEID , "cover_polling_time:250"] ,
    ["radiolog/%s/cfg/read" % NODEID  , "cover_polling_time"] ,
]


def on_message(client, userdata, msg):
    if NODEID in msg.topic:
        print(msg.topic, " ", str(msg.payload))


if key == "log":
    client.on_message = on_message
    client.loop_forever()
client.loop_start()

if key == "cfg":
    for i in cmd_table_cfg:
        client.publish(i[TOPIC], i[PAYLOAD])
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
