<?php

# user/password needed to access this page, if you don't want any
# user/pass, just comment out these lines
$PHP_USER="user";
$PHP_PASSWORD="pass";

# for communication with irssi's mobile module
$IRSSI_PORT=7070;
$IRSSI_ADDRESS="127.0.0.1";
# this should be the same as /SET mobile_password
$IRSSI_PASS="";

# comment out if you want HTML instead of WML
#$WML=1;

if ($WML) Header("Content-Type: text/vnd.wap.wml");

if ($PHP_USER && !isset($PHP_AUTH_USER)) {
        # ask user/pass
        Header("WWW-Authenticate: basic realm=\"irssiwap\"");
        Header("HTTP/1.0 401 Unauthorized");
}

# check that yer/password matches
if ($PHP_USER && ($PHP_USER != $PHP_AUTH_USER ||
		  $PHP_PASSWORD != $PHP_AUTH_PW)) {
	Header("HTTP/1.0 401 Unauthorized");
	return;
}

$fp = fsockopen($IRSSI_ADDRESS, $IRSSI_PORT);
if (!$fp) return;

fputs($fp, "$IRSSI_PASS\n");

if ($WML) fputs($fp, "lang wml\n");
if ($server) fputs($fp, "server $server\n");
if ($channel) fputs($fp, "channel $channel\n");
if ($msg) fputs($fp, "msg $msg\n");
fputs($fp, "page\n");

while (!feof($fp)) {
	$data = fread($fp, 1024);
	print $data;
}

fclose($fp);

?>
