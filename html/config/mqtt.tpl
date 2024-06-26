<html><head><title>MQTT settings</title>
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
<link rel="stylesheet" type="text/css" href="../style.css">
<script type="text/javascript">

window.onload=function(e) {
	sc('mqtt-enable',%mqtt-enable%);
	sc('mqtt-use-ssl',%mqtt-use-ssl%);
	di();
};

function sc(l,v) {
document.getElementById(l).checked=v;}

function sd(l,v) {
if(document.getElementById(l)) document.getElementById(l).disabled=v;}

function di(){
var v=true;
if (document.getElementById('mqtt-enable').checked) v=false;
sd('mqtt-host',v);
sd('mqtt-port',v);
sd('mqtt-keepalive',v);
sd('mqtt-devid',v);
sd('mqtt-user',v);
sd('mqtt-pass',v);
sd('mqtt-use-ssl',v);
sd('mqtt-relay-subs-topic',v);
sd('mqtt-temp-subs-topic',v);
sd('mqtt-temp-timeout-secs',v);
sd('mqtt-dht22-temp-pub-topic',v);
sd('mqtt-dht22-temp-timeout-topic',v);
sd('mqtt-dht22-humi-pub-topic',v);
sd('mqtt-ds18b20-temp-pub-topic',v);
sd('mqtt-state-pub-topic',v);

}

</script>

</head>
<body>
<div id="main">
<p>
<b>MQTT Settings</b>
</p>
<form name="mqttform" action="mqtt.cgi" method="post">

<table>
<tr><td>MQTT enabled?:</td><td><input type="checkbox" name="mqtt-enable" id="mqtt-enable" onclick="di();"/></td></tr>
<tr><td>Host:</td><td><input type="text" name="mqtt-host" id="mqtt-host" value="%mqtt-host%"/>     </td></tr>
<tr><td>Port:</td><td><input type="text" name="mqtt-port" id="mqtt-port" value="%mqtt-port%"/>     </td></tr>
<tr><td>Keepalive (sec):</td><td><input type="text" name="mqtt-keepalive" id="mqtt-keepalive" value="%mqtt-keepalive%"/> </td></tr>
<tr><td>Device ID:</td><td><input type="text" name="mqtt-devid" id="mqtt-devid" value="%mqtt-devid%"/>   </td></tr>
<tr><td>User:</td><td><input type="text" name="mqtt-user" id="mqtt-user" value="%mqtt-user%"/>     </td></tr>
<tr><td>Password:</td><td><input type="password" name="mqtt-pass" id="mqtt-pass" value="%mqtt-pass%"/>     </td></tr>
<tr><td>Use SSL?:</td><td><input type="checkbox" name="mqtt-use-ssl" id="mqtt-use-ssl" /> (max 1024 bit key size)  </td></tr>
<tr><td>Relays subs topic:</td><td><input type="text" name="mqtt-relay-subs-topic" id="mqtt-relay-subs-topic" value="%mqtt-relay-subs-topic%">     </td></tr>
<tr><td>Temp subs topic:</td><td><input type="text" name="mqtt-temp-subs-topic" id="mqtt-temp-subs-topic" value="%mqtt-temp-subs-topic%">     </td>
<td>
If thermostat is configured to use MQTT as temperature source use this topic.
</td> </tr>
<tr><td>Mqtt Temperature timeout:</td><td><input type="text" name="mqtt-temp-timeout-secs" id="mqtt-temp-timeout-secs" value="%mqtt-temp-timeout-secs%">     </td>
<td>Timeout for Temp subs topic reception. Temp subs topic should be published &lt; every timeout seconds or thermostat turns off</td></tr>

<tr><td>DHT22 temp pub topic:</td><td><input type="text" name="mqtt-dht22-temp-pub-topic" id="mqtt-dht22-temp-pub-topic" value="%mqtt-dht22-temp-pub-topic%">     </td></tr>

<tr><td>DHT22 humi pub topic:</td><td><input type="text" name="mqtt-dht22-humi-pub-topic" id="mqtt-dht22-humi-pub-topic" value="%mqtt-dht22-humi-pub-topic%">     </td></tr>
<tr><td>DS18B20 temp pub topic:</td><td><input type="text" name="mqtt-ds18b20-temp-pub-topic" id="mqtt-ds18b20-temp-pub-topic" value="%mqtt-ds18b20-temp-pub-topic%">     </td></tr>
<tr><td>State pub topic:</td><td><input type="text" name="mqtt-state-pub-topic" id="mqtt-state-pub-topic" value="%mqtt-state-pub-topic%">     </td>
<td>System state published to MQTT topic</tr>
<tr><td><button type="button" onClick="parent.location='/'">Back</button><input type="submit" name="save" value="Save"></td></tr>
</table>
</form>

</body>
</html>
