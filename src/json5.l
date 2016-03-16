# \w  Alphanumeric string
# \a  Alphanumeric string including '_'
# \d  Decimal digit
# \h  Hexadecimal digit
# +   Repeat

\s+            WHITESPACE

\a+            NAME

\d+            INT
\+\d+          INT
\-\d+          INT

0x\h+          INT

.\d+           FLOAT
\+.\d+         FLOAT
\-.\d+         FLOAT

\d+.           FLOAT
\+\d+.         FLOAT
\-\d+.         FLOAT

.\d+e\d+       FLOAT
.\d+e\+\d+     FLOAT
.\d+e\-\d+     FLOAT

\d+.e\d+       FLOAT
\d+.e\+\d+     FLOAT
\d+.e\-\d+     FLOAT

\+\d+.e\d+     FLOAT
\+\d+.e\+\d+   FLOAT
\+\d+.e\-\d+   FLOAT

\-\d+.e\d+     FLOAT
\-\d+.e\+\d+   FLOAT
\-\d+.e\-\d+   FLOAT

{              OBJECT_START
}              OBJECT_END

[              ARRAY_START
]              ARRAY_END

"              STRING_START
'              STRING_START

,              COMMA
:              COLON

/*             COMMENT_START
//             COMMENT_START
