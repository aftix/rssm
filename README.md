<b>rssm</b><br>
A simple rss feed manager that depends on libcurl and libxml2.
It can either run as a daemon or in terminal.

Rssm reads a feedlist (default: $XDG_CONFIG_HOME/rssm.conf). It then sets up a directory (default $HOME/rss) to place the rss data
into.
An example line of the feedlist file is:<br><br>

&lt;RSSTAG&gt; &lt;URL&gt; #Comment<br><br>

Rssm creates a regular file in the set directory named &lt;RSSTAG&gt; and "&lt;RSSTAG&gt; desc". The description of an rss channel will be put
into "&lt;RSSTAG&gt; desc", appending the new descriptions to the end as they come in. A line in &lt;RSSTAG&gt; desc could be:<br><br>
title: An rss Feed<br><br>

If there are multiple lines with the same name, the newest is always the bottom most one in the file. Rssm does not replicate information
to the desc file - only new information not already there will be appended.
Rssm appends any new rss items to &lt;RSSTAG&gt; in reverse order. Items are seperated by "\nITEMS\n"
An example is<br><br>

&lt;item info&gt;<br>
ITEMS<br>
&lt;item info&gt;<br>
ITEMS<br><br>

Rssm will not append duplicate information (determined by link).
Every tag in the feedlist file will have its own item file and desc file.
By default rssm logs to ~/.rssmlog .
