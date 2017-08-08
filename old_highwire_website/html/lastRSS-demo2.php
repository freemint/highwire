

<span class="forum1">
<?php

function ShowOneRSS($url) {
    global $rss;
    if ($rs = $rss->get($url)) {
        echo '<span class="forum2"><a href="'.$rs['link'].'">'.$rs['title']."</a></span>\n";
        echo '<!--<small>Language/Encoding: <font color="red"><b>'.$rs['language'].'/'.$rs['encoding']."</b></font><br></small>-->\n";
        
            echo "<br>\n";
            foreach ($rs['items'] as $item) {
                echo '<a href="'.$item['link'].'" title="'.$item['description'].'">&bull;&nbsp;'.$item['title'].'</a><br>';
            }
            if ($rs['items_count'] <= 0) { echo "<li>Sorry, no items found in the RSS file :-(</li>"; }
            echo "<br>\n";
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

</span>
