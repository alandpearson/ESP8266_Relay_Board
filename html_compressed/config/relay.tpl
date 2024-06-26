<html><head><title>Relay Settings</title>
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
<meta http-equiv="Content-Type" content="text/html;charset=utf-8">
<link rel="stylesheet" type="text/css" href="../style.css">
</head>
<body>
<div id="main">
<p>
<b>Relay Settings</b>
</p>
<form name="relayform" action="relay.cgi" method="post">

<table>
<tr>
<td>Relay latching enabled?:</td><td><input type="checkbox" name="relay-latching-enable" id="relay-latching-enable" %relay-latching-enable% /></td>
<td></td>
</tr>
<tr>
<td>Relay 1 name:</td><td><input type="text" name="relay1name" id="relay1name" value="%relay1name%"/></td>
<td>Thermostat controlled ? <input type="checkbox" name="relay1thermostat" id="relay1thermostat" %relay1thermostat%></input></td>
</tr>
<tr>
<td>Relay 2 name:</td><td><input type="text" name="relay2name" id="relay2name" value="%relay2name%"/></td>
<td>Thermostat controlled ? <input type="checkbox" name="relay2thermostat" id="relay2thermostat" %relay2thermostat%></input></td>
</tr>
<tr>
<td>Relay 3 name:</td><td><input type="text" name="relay3name" id="relay3name" value="%relay3name%"/></td>
<td>Thermostat controlled ? <input type="checkbox" name="relay3thermostat" id="relay3thermostat" %relay3thermostat%></input></td>
</tr>
<tr>
<td>Relay 1 Thermostat Rest Minutes:</td><td><input type="text" name="therm-relay-rest-min" id="therm-relay-rest-min" value="%therm-relay-rest-min%"/></td>
<td>To avoid relay cycling, this the minimum OFF period - only applicable to thermostat operation on relay 1.</td>
</tr>
</table>
<br/>
<button type="button" onClick="parent.location='/'">Back</button>
<input type="submit" name="save" value="Save">
</p>
</form>

</body>
</html>
