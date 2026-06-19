<?php
header('Content-Type: text/plain');
echo "Hello from PHP CGI\n";
echo "Method: " . $_SERVER['REQUEST_METHOD'] . "\n";
echo "Query: " . $_SERVER['QUERY_STRING'] . "\n";
?>