<?
function ImageState($file) {
global $section;


if ($section == $file)
	{
	} else {
echo "2";
	}
} ?>
<?
function SubStat($file) {
global $entry;

if ($entry == $file)
	{
return "1";
	} else {
return "2";
	}
} ?><? 
if ($section == "home")
	{
        $home_str = "<br><a class='submenu".SubStat("welcome")."' href='http://highwire.atari-users.net/_TesT_/index.php?section=home&entry=welcome'>News</a> <a class='submenu".SubStat("oldnews")."' href='http://highwire.atari-users.net/_TesT_/index.php?section=home&entry=oldnews'>Archive</a>";
	} 
else if ($section == "dev")
{
$dev_str = "<br><a class='submenu".SubStat("develop")."' href='http://highwire.atari-users.net/_TesT_/index.php?section=dev&entry=develop'>Info</a> <a class='submenu".SubStat("coders")."' href='http://highwire.atari-users.net/_TesT_/index.php?section=dev&entry=coders'>Coders</a> <a class='submenu".SubStat("hwdom")."' href='http://highwire.atari-users.net/_TesT_/index.php?section=dev&entry=hwdom'>HighWire DOM</a>";
}
else if ($section == "snap")
{
$snap_str = "<br><a class='submenu".SubStat("1")."' href='http://highwire.atari-users.net/_TesT_/index.php?section=snap&entry=1'>[1]</a> <a class='submenu".SubStat("2")."' href='http://highwire.atari-users.net/_TesT_/index.php?section=snap&entry=2'>[2]</a>";
}?>
<table hspace="0" class="topmenu" border="0"><tr valign="top"><td><img src="./images/1x1transp.gif" width="3" height="1"></td><td><a class="menu<? ImageState("home") ?>" target="_top" href="./index.php?section=home&entry=welcome">Home</a>&nbsp;&nbsp;<? echo $home_str ?></td><td><a class="menu<? ImageState("docs") ?>" target="_top" href="./index.php?section=docs">Documentation</a>&nbsp;&nbsp;</td><td><a class="menu<? ImageState("snap") ?>" target="_top" href="./index.php?section=snap&entry=1">Screenshots</a>&nbsp;&nbsp;<? echo $snap_str; ?></td><td><a class="menu<? ImageState("file") ?>" target="_top" href="./index.php?section=file">Download</a>&nbsp;&nbsp;</td><td><a class="menu<? ImageState("dev") ?>" target="_top" href="./index.php?section=dev&entry=develop">Development</a>&nbsp;&nbsp;<? echo $dev_str; ?></td><td><img src="./images/1x1transp.gif" width="1" height="32"></td></tr></table>

