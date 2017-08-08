
<?php
// include lastRSS library
include './lastRSS.php';

// create lastRSS object
$rss = new lastRSS;

// setup transparent cache
$rss->cache_dir = './rsscache';
$rss->cache_time = 3600; // one hour
$rss->cp = 'UTF-8';
$rss->items_limit = 5; 

// Try to load and parse RSS file
if ($rs = $rss->get('http://www.atariforums.com/rss.php?21')) {
$rss->items_limit = 5; 
    // Show website logo (if presented)
    if ($rs[image_url] != '') {
        echo "<a href=\"$rs[image_link]\"><img src=\"$rs[image_url]\" alt=\"$rs[image_title]\" vspace=\"1\" border=\"0\" /></a><br />\n";
        }
    // Show clickable website title
    echo "<big><b><a href=\"$rs[link]\">$rs[title]</a></b></big><br />\n";
    // Show website description
    echo "$rs[description]<br />\n";
    // Show last published articles (title, link, description)
    echo "<ul>\n";
    foreach($rs['items'] as $item) {
        echo "\t<li><a href=\"$item[link]\">".$item['title']."</a><br />".$item['description']."</li>\n";
        }
    echo "</ul>\n";
    }
else {
    echo "Error: It's not possible to reach RSS file...\n";
}


// setup transparent cache
$rss->cache_dir = './rsscache';
$rss->cache_time = 3600; // one hour
$rss->cp = 'UTF-8';
$rss->items_limit = 5; 



// Try to load and parse RSS file
if ($rs = $rss->get('http://www.atariforums.com/rss.php?23')) {
    // Show website logo (if presented)
    if ($rs[image_url] != '') {
        echo "<a href=\"$rs[image_link]\"><img src=\"$rs[image_url]\" alt=\"$rs[image_title]\" vspace=\"1\" border=\"0\" /></a><br />\n";
        }
    // Show clickable website title
    echo "<big><b><a href=\"$rs[link]\">$rs[title]</a></b></big><br />\n";
    // Show website description
    echo "$rs[description]<br />\n";
    // Show last published articles (title, link, description)
    echo "<ul>\n";
    foreach($rs['items'] as $item) {
        echo "\t<li><a href=\"$item[link]\">".$item['title']."</a><br />".$item['description']."</li>\n";
        }
    echo "</ul>\n";
    }
else {
    echo "Error: It's not possible to reach RSS file...\n";
}

?>