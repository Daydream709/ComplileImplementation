%filenames = "scanner"

 /*
  * Please don't modify the lines above.
  */

 /* You can add lex definitions here. */
digit [0-9]
letter [a-zA-Z]

%x COMMENT STR IGNORE

%%

 /*
  * Below is examples, which you can wipe out
  * and write regular expressions and actions of your own.
  *
  * All the tokens:
  *   Parser::ID
  *   Parser::STRING
  *   Parser::INT
  *   Parser::COMMA
  *   Parser::COLON
  *   Parser::SEMICOLON
  *   Parser::LPAREN
  *   Parser::RPAREN
  *   Parser::LBRACK
  *   Parser::RBRACK
  *   Parser::LBRACE
  *   Parser::RBRACE
  *   Parser::DOT
  *   Parser::PLUS
  *   Parser::MINUS
  *   Parser::TIMES
  *   Parser::DIVIDE
  *   Parser::EQ
  *   Parser::NEQ
  *   Parser::LT
  *   Parser::LE
  *   Parser::GT
  *   Parser::GE
  *   Parser::AND
  *   Parser::OR
  *   Parser::ASSIGN
  *   Parser::ARRAY
  *   Parser::IF
  *   Parser::THEN
  *   Parser::ELSE
  *   Parser::WHILE
  *   Parser::FOR
  *   Parser::TO
  *   Parser::DO
  *   Parser::LET
  *   Parser::IN
  *   Parser::END
  *   Parser::OF
  *   Parser::BREAK
  *   Parser::NIL
  *   Parser::FUNCTION
  *   Parser::VAR
  *   Parser::TYPE
  */

 /* reserved words */
"array" {adjust(); return Parser::ARRAY;}

 /* TODO: Put your lab2 code here */
","  {adjust(); return Parser::COMMA;}
":"  {adjust(); return Parser::COLON;}
";"  {adjust(); return Parser::SEMICOLON;}
"("  {adjust(); return Parser::LPAREN;}
")"  {adjust(); return Parser::RPAREN;}
"["  {adjust(); return Parser::LBRACK;}
"]"  {adjust(); return Parser::RBRACK;}
"{"  {adjust(); return Parser::LBRACE;}
"}"  {adjust(); return Parser::RBRACE;}
"."  {adjust(); return Parser::DOT;}
"+"  {adjust(); return Parser::PLUS;}
"-"  {adjust(); return Parser::MINUS;}
"*"  {adjust(); return Parser::TIMES;}
"/"  {adjust(); return Parser::DIVIDE;}
"="  {adjust(); return Parser::EQ;}
"<>" {adjust(); return Parser::NEQ;}
"<"  {adjust(); return Parser::LT;}
"<=" {adjust(); return Parser::LE;}
">"  {adjust(); return Parser::GT;}
">=" {adjust(); return Parser::GE;}
"&" {adjust(); return Parser::AND;}
"|" {adjust(); return Parser::OR;}
":=" {adjust(); return Parser::ASSIGN;}
"if" {adjust(); return Parser::IF;}
"then" {adjust(); return Parser::THEN;}
"else" {adjust(); return Parser::ELSE;}
"while" {adjust(); return Parser::WHILE;}
"for" {adjust(); return Parser::FOR;}
"to" {adjust(); return Parser::TO;}
"do" {adjust(); return Parser::DO;}
"let" {adjust(); return Parser::LET;}
"in" {adjust(); return Parser::IN;}
"end" {adjust(); return Parser::END;}
"of" {adjust(); return Parser::OF;}
"break" {adjust(); return Parser::BREAK;}
"nil" {adjust(); return Parser::NIL;}
"function" {adjust(); return Parser::FUNCTION;}
"var" {adjust(); return Parser::VAR;}
"type" {adjust(); return Parser::TYPE;}

/* 注释 */
"/*" {
    adjust();
    comment_level_ = 1;
    begin(StartCondition_::COMMENT); // 进入 COMMENT 独占状态
}

/* 在 COMMENT 状态下处理内部嵌套与结束 */
<COMMENT>{
    "/*" {
        adjust();
        comment_level_++;
    }
    
    "*/" {
        adjust();
        comment_level_--;
        if (comment_level_ == 0) {
            begin(StartCondition_::INITIAL);
        }
    }
    
    \n {
        adjust();
        errormsg_->Newline();
    }
    
    . {
        adjust();
    }
}

/* 字符串 */
\" {
  adjust(); 
  string_buf_.clear();
  begin(StartCondition_::STR); 
}
<STR> {
  /* 遇到右端双引号，闭合字符串 */
  \" {
    adjustStr(); 
    begin(StartCondition_::INITIAL); // 退出字符串状态
    setMatched(string_buf_);
    return Parser::STRING;
  }
  /* 转义序列 */
  \\n { adjustStr(); string_buf_ += '\n'; }
  \\t { adjustStr(); string_buf_ += '\t'; }
  \\\" { adjustStr(); string_buf_ += '\"'; }
  \\\\ { adjustStr(); string_buf_ += '\\'; }

  /* 匹配 \ddd (三位十进制数字 ASCII 码) */
  \\[0-9]{3} {
    adjustStr();
    int val = std::stoi(matched().substr(1)); // 提取数字部分并转成int
    if (val > 255) {
      errormsg_->Error(errormsg_->tok_pos_, "ASCII out of range");
    }
    string_buf_ += static_cast<char>(val);
  }

  /* 控制字符 \^c */
  \\\^[@A-Z\[\\\]\^_] {
    adjustStr();
    // ASCII 里，^C 的值等价于 'C' - '@' 的偏移量，此为经典位运算解法
    string_buf_ += (matched()[2] - '@'); 
  }

  /* 多行忽略结构，遇到第一个 \ 时进入 IGNORE 状态 */
  \\ { 
    adjustStr(); 
    begin(StartCondition_::IGNORE); 
  }

  /* 错误：字符串内出现了真实的非转义换行 */
  \n {
    adjust();
    errormsg_->Error(errormsg_->tok_pos_, "newline in string"); 
    errormsg_->Newline();
    begin(StartCondition_::INITIAL);
  }

  /* 其他任何普通合法的单个字符 */
  . { 
    adjustStr(); 
    string_buf_ += matched(); // 存进去！
  }
}

/* 匹配忽略多行的语法 */
<IGNORE> {
  /* 遇到了结尾的反斜杠，回归 STR 状态 */
  \\ { 
    adjustStr(); 
    begin(StartCondition_::STR); 
  }
  
  /* 中间的空白符只adjustStr()和记录换行 */
  [ \t\n\f] {
    adjustStr();
    if (matched() == "\n") errormsg_->Newline();
  }
  
  /* 不可出现非空白字符 */
  . {
    errormsg_->Error(errormsg_->tok_pos_, "illegal character in ignore sequence"); 
    adjustStr();
  }
}

{digit}+ {adjust(); return Parser::INT;}
{letter}({letter}|{digit}|_)* {adjust(); return Parser::ID;}


<COMMENT><<EOF>> {
  adjust();
  errormsg_->Error(errormsg_->tok_pos_, "unclosed comment");
  return 0;
}

<STR><<EOF>> {
  adjust();
  errormsg_->Error(errormsg_->tok_pos_, "unclosed string");
  return 0;
}
 /*
  * skip white space chars.
  * space, tabs and LF
  */
[ \t]+ {adjust();}
\n {adjust(); errormsg_->Newline();}

 /* illegal input */
. {adjust(); errormsg_->Error(errormsg_->tok_pos_, "illegal token");}