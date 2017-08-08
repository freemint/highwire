<!-- navigation bar -->
<?
function ImageState($file) {
   $theuri = $_SERVER['REQUEST_URI'];
if ($theuri == "/")
	{
$theuri = "/index.php";
	}

	$x=strpos($theuri,$file);
	$l = strlen($x);
   $images = array();
   $new = array();

if ($l == "0")
	{
	} else {
echo "2";
	}
} ?>
<? $loc = "http://highwire.atari-users.net"; ?>
<table cellspacing="0" cellpadding="2" width="100%" border="0" bgcolor="#02B402" background="<? echo $loc; ?>/images/xa_banner.png">
<tr valign="top" align="center"><td colspan="1"><img src="<? echo $loc; ?>/images/xaaes_top.gif"></td></tr>
<tr><td>

<table cellspacing="0" cellpadding="2" border="0">
<tr>
<td width="20%" align="center"><a target="_top" href="<? echo $loc; ?>/index.php"><img src="<? echo $loc; ?>/images/b_home<? ImageState("/index.php") ?>.gif" width="102" height="22" alt="Home" border="0"></a></td>
<td width="20%" align="center"><a target="_top" href="<? echo $loc; ?>/docs.php?chapter=contents.htm"><img src="<? echo $loc; ?>/images/b_docs<? ImageState("/docs.php") ?>.gif" width="102" height="22" alt="Documentation" border="0"></a></td>

<td width="20%" align="center"><a target="_top" href="<? echo $loc; ?>/download.php?product=xaaes"><img src="<? echo $loc; ?>/images/b_products<? ImageState("/download.php") ?>.gif" width="102" height="22" alt="Products" border="0"></a></td>
<td width="20%" align="center"><a target="_top" href="<? echo $loc; ?>/history.php?xaaes=background"><img src="<? echo $loc; ?>/images/b_history<? ImageState("/history.php") ?>.gif" width="102" height="22" alt="History" border="0"></a></td>
<td width="20%" align="center"><a target="_top" href="<? echo $loc; ?>/screenshots.php"><img src="<? echo $loc; ?>/images/b_screenshots<? ImageState("/screenshots.php") ?>.gif" width="102" height="22" alt="Screenshots" border="0"></a></td>
<td width="20%" align="center"><a target="_top" href="http://www.atariforums.com/list.php?26"><img src="<? echo $loc; ?>/images/b_forum<? ImageState("/navbar.php") ?>.gif" width="102" height="22" alt="Forum" border="0"></a></td>
<? 
$theuri = $_SERVER['REQUEST_URI'];
echo $theuri; 

?>
</td></tr></table>

</td>
</tr></table>
<!-- navigation bar end -->

<!--? echo $_SERVER['REQUEST_URI']; ?-->