<?php
function ImageState($file) {
global $section;


if ($section == $file)
	{
	} else {
echo "2";
	}
} ?>
<?php
function SubStat($file) {
global $entry;

if ($entry == $file)
	{
return "1";
	} else {
return "2";
	}
} ?><?php 
if ($section == "home")
	{
        $home_str = "<br><a class='submenu".SubStat("welcome")."' href='./index.php?section=home&entry=welcome'>News</a> <a class='submenu".SubStat("oldnews")."' href='./index.php?section=home&entry=oldnews'>Archive</a>";
	} 
else if ($section == "dev")
{
$dev_str = "<br><a class='submenu".SubStat("develop")."' href='./index.php?section=dev&entry=develop'>Info</a> <a class='submenu".SubStat("coders")."' href='./index.php?section=dev&entry=coders'>Coders</a> <a class='submenu".SubStat("hwdom")."' href='./index.php?section=dev&entry=hwdom'>HighWire DOM</a>";
}
else if ($section == "snap")
{
$snap_str = "<br><a class='submenu".SubStat("1")."' href='./index.php?section=snap&entry=1'>[1]</a> <a class='submenu".SubStat("2")."' href='./index.php?section=snap&entry=2'>[2]</a>";
}?>
<table hspace="0" class="topmenu" border="0"><tr valign="top"><td><img src="./images/1x1transp.gif" width="3" height="1"></td><td><a class="menu<?php ImageState("home") ?>" target="_top" href="./index.php?section=home&entry=welcome">Home</a>&nbsp;&nbsp;<?php echo $home_str ?></td><td><a class="menu<?php ImageState("docs") ?>" target="_top" href="./index.php?section=docs">Documentation</a>&nbsp;&nbsp;</td><td><a class="menu<?php ImageState("snap") ?>" target="_top" href="./index.php?section=snap&entry=1">Screenshots</a>&nbsp;&nbsp;<?php echo $snap_str; ?></td><td><a class="menu<?php ImageState("file") ?>" target="_top" href="./index.php?section=file">Download</a>&nbsp;&nbsp;</td><td><a class="menu<?php ImageState("dev") ?>" target="_top" href="./index.php?section=dev&entry=develop">Development</a>&nbsp;&nbsp;<?php echo $dev_str; ?></td><td><img src="./images/1x1transp.gif" width="1" height="32"></td></tr></table>

