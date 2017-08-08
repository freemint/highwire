<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">

<title>::: HighWire Development Project - highwire.atari-users.net :::</title>

<LINK href="./stylesheet.css" type=TEXT/CSS rel=stylesheet>
</head>


<? $first = $_GET['first'];
$section = $_GET['section'];

if ($first == "")
		{
$first = 0;
		}
?>


<? $limit = $_GET['limit'];

if ($limit == "")
		{
$limit = 3;
		}
?>
<center>
<!--table cellspacing="0" cellpadding="0" width="100%">
<tr><td width="50%" valign="top">

<!-- old table placeholder -->


</td>


<td-->

<!-- INSIDE MAIN TABLE -->

<table cellspacing="0" cellpadding="5" border="0" width="980" style="BORDER: #000000 1px solid;" bgcolor="white">
<tr><td colspan="3" align="center">
<img src="./images/logotst2.gif">

</td></tr>
<tr><td valign="top" width="205">
<!-- span1a -->

<table cellspacing="0" cellpadding="0" border="0" align="left">
<tr><td bgcolor="white" valign="top">&nbsp;<br>
<?php include 'http://highwire.atari-users.net/rss_forum.php'; ?> 

</td></tr>
<tr><td></td></tr>
<tr><td bgcolor="white" valign="top">

<?php include 'http://highwire.atari-users.net/rss_mantis.php'; ?> 

</td></tr>
</table>






</td>

<!-- span1b -->
<!-- MAIN AREA START -->

<? 
$section = $_GET['section'];
$entry = $_GET['entry'];

?>
<?
if ($section == "")
	{
$section = "home";
	}
?>
<td valign="top" align="left" class="entries">
<?php include './include/menu.php'; ?> 

<table cellspacing="0" cellpadding="0" border="0" width="100%">

  <tr valign="top">

    <td width="20" colspan="3"><img src="images/up_left.gif"></td>
    <td><img height="4" width="729" src="images/1x1orange.gif"></td>
  </tr>
  <tr valign="top">
    <td height="100%" bgcolor="#ffc66b" width="1"><img width="1" src="./images/1x1_transp.gif"></td>
    <td height="100%" bgcolor="#ffa000" width="3"><img width="1" src="./images/1x1_transp.gif"></td>
    <td width="16"><img height="1" width="1" src="./images/1x1_transp.gif"></td>
    <td>

      <!--contents starts here -->



<?
if ($section == "home")
	{

	if ($entry == "oldnews")
		{
		include './include/oldnews.php';
		}
	else	{
		include './include/welcome.php';
		}

	} 
else if ($section == "docs")
	{
ob_start();
include './include/hw_doc.htm';
$output = ob_get_contents();
ob_end_clean();

echo iconv("ISO-8859-1", "UTF-8", $output);
	}
else if ($section == "snap")
	{
include './include/screenshots.php';
	}
else if ($section == "file")
	{
include './include/download.php';
	}
else if ($section == "dev")
	{

	if ($entry == "coders")
		{
		include './include/coders.php';
		}
	else if ($entry == "hwdom")
		{
		echo "<pre>";
		include './include/hwdom.txt';
		echo "</pre>";
		}
	else 
		{
		include './include/develop.php';
		}

	}
?>

      <!--contents ends here -->

    </td></tr></table>












<!-- MAIN AREA END -->

</td>

</tr>
<tr><td colspan="3" align="center">
<?php include './include/footer.htm'; ?> 



</td></tr>

</table>
</center>

<!--/td>
<!-- OUTSIDE MAIN TABLE -->

</td><td width="50%">Right</td></tr></table-->

</body>
</html>