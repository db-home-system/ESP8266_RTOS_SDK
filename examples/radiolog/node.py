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

def on_message(client, userdata, msg):
    print(msg.topic, " ", str(msg.payload))

client = mqtt.Client()
client.username_pw_set("radiolog", "home24radiolog98")
client.on_connect = on_connect
client.on_message = on_message

client.connect("fagotto.asterix.cloud", 1317, 60)
client.loop_start()
#NODEID = "Node_85d904"
NODEID = "Node_f4a98f"
TOPIC=0
PAYLOAD=1

cmd_table = {
    "reset" : ["radiolog/%s/reset" % NODEID, ""],
    "open"  : ["radiolog/%s/cover/set" % NODEID, "open"],
    "close" : ["radiolog/%s/cover/set" % NODEID, "close"],
    "go"    : ["radiolog/%s/cover/set_position" % NODEID, "ARGS"],
    "r"     : ["radiolog/%s/cfg/read" % NODEID, "ARGS"],
    "w"     : ["radiolog/%s/cfg/write" % NODEID, "ARGS"],
    "dump"  : ["radiolog/%s/cfg/dump" % NODEID, ""],
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

if len(sys.argv) < 2:
    print("usage %s cmd <%s>" % (sys.argv[0], list(cmd_table.keys())))
    sys.exit(0)

key = sys.argv[1].strip()
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
            payload = sys.argv[2]
    except IndexError:
        print("Missing args")
        sys.exit(1)

    client.publish(cmd[TOPIC], payload)
    time.sleep(1)
