


<?php


	function Truncate ($str, $length=30, $trailing='...')
{
      // take off chars for the trailing
   
      if (strlen($str) > $length+3)
      {
         // string exceeded length, truncate and add trailing dots
         return substr($str,0,$length).$trailing;
      }
      else
      {
         // string was already short enough, return the string
         $res = $str;
      }
  
      return $res;
}

function ShowOneRSS($url) {
    global $rss;
    if ($rs = $rss->get($url)) {
        echo '<a class="forum2" href="'.$rs['link'].'">'.$rs['title']."</a>\n";
        echo '<!--<small>Language/Encoding: <font color="red"><b>'.$rs['language'].'/'.$rs['encoding']."</b></font><br></small>-->\n";
        
            echo "<br>\n";
            foreach ($rs['items'] as $item) {
                echo '<a class="forum1" href="'.$item['link'].'" title="'.$item['description'].'">&bull;'.Truncate($item['title']).'</a><br>';
            }
            if ($rs['items_count'] <= 0) { echo "<p>Sorry, no items found in the RSS file :-(</p>"; }
            
    }
}

// ===============================================================================

// include lastRSS
include "./lastRSS.php";

// List of RSS URLs
$rss_left = array(
    'http://www.atariforums.com/rss.php?21',
    'http://www.atariforums.com/rss.php?23',
    'http://www.atariforums.com/rss.php?22'
);


// Create lastRSS object
$rss = new lastRSS;

// Set cache dir, cache interval and character encoding
$rss->cache_dir = './rsscache';
$rss->cache_time = 1800; // (30 minutes)
$rss->cp = 'UTF-8';
$rss->items_limit = 5;

// Show all rss files

foreach ($rss_left as $url) ShowOneRSS($url);

?>

