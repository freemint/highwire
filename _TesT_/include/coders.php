
<!--contents starts here -->


<P><br>

<table width="80%"><tr valign="top"><td width="50%">

<font face="verdana" size="2">

<B>Getting started</B>

<p align="left"><font size="2">
Right, you want to do some coding for the HighWire project and wonder where to start? Find a few essential guidelines in 
<A href="./code_faq.txt">HighWire coders FAQ</a>.

</font></p>



<b>HighWire DOM</b>
<p align="left"><font size="2">
If you need information regarding the internal document structures of HighWire along with some comments on implementations, then <a href="./index.php?section=dev&entry=hwdom">HighWire DOM</a> is for you. It also covers the correspondences between
HighWire's internal storage of a Document and the <a href="http://www.w3.org/TR/2002/CR-DOM-Level-2-HTML-20020605/">W3 specs
for the Document Object Model</a>.</font></p>



<b>CVS</b>
<p align="left"><font size="2">
The CVS server is accessed through the "pserver" method.<br>
Now open a shell and type:<br>
&nbsp;<br><code>
&nbsp;&nbsp;&nbsp;&nbsp;export CVSROOT=:pserver:cvsanon@highwire.atari-users.net:/atariforge<br>
&nbsp;&nbsp;&nbsp;&nbsp;cvs login<br>
&nbsp;<br></code>
(Logging in)<br>
&nbsp;<br>
<code>
&nbsp;&nbsp;&nbsp;&nbsp;CVS password:</code><br>
&nbsp;<br>
Now type 'cvsanon' as password. If all is ok you get a prompt without any additional message. If something goes wrong you get an error message.<br>
&nbsp;<br>
Now you can checkout a working copy of the current HighWire branch with:<br>
&nbsp;<br>
<code>
&nbsp;&nbsp;&nbsp;&nbsp;cvs checkout highwire<br></code>
</font></p>To view the CVS tree with your browser, use this URL:<br>
<a href="http://highwire.atari-users.net/cgi-bin/cvsweb.cgi/highwire/?sortby=date#dirlist">http://highwire.atari-users.net/cgi-bin/cvsweb.cgi/highwire/?sortby=date#dirlist</A>
</font>

</td><td>

<font face="verdana" size="2">
<b>Libraries</b>
<p align="left"><font size="2">
As the HighWire sources can be compiled on a variety of C-compilers, you will need to get libraries for the one of your choice.
<br>&nbsp;<br>

<b>GCC</b> (<font size="1"><a href="http://sparemint.atariforge.net/sparemint/html/packages/gemlib.html">gemlib</a>, <a href="http://sparemint.atariforge.net/sparemint/html/packages/libungif-devel.html">ungif</A></font>)<br>


<b>Pure-C</b> (<font size="1"><A HREF="./libs/purec/gem422pc.zip">gemlib</A>, <A HREF="./libs/purec/pcungif.zip">ungif</A>, <a href="./libs/purec/pcpng125.zip">png</a>, <a href="./libs/purec/pcjpeg6b.zip">jpg</a>, <A HREF="./libs/purec/zlib_pc.zip"><s>zlib</s></A>, <A HREF="./libs/purec/tifflib_pc.zip"><s>tifflib</s></A></font>)<br>


<b>Lattice-C</b> (<font size="1"><A HREF="./libs/lattice/gemlib.zip">gemlib</A>, <A HREF="./libs/lattice/ungiflib.zip">ungif</A>, <A HREF="./libs/lattice/zlib.zip"><s>zlib</s></A>, <s>pnglib</s></font>)
<br>&nbsp;<br>




</font></p>

<b>Developers mailing list</b>
<p align="left"><font size="2">If in any way involved in developing HighWire you should of course join the <a href="http://atari-users.net/mailman/listinfo/highwire_atari-users.net">developers mailing list</a> to get up-to-date with everything about HighWire.</font></p>

</font>

</td></tr></table>


<!--contents ends here -->