#include "tGraphicScreen.h"

void setupPS2(uint8_t kb_type);
uint8_t ps2read();
void    endPS2();


//
// カーソルの移動
// ※pos_x,pos_yは本関数のみでのみ変更可能
//
void tGraphicScreen::MOVE(uint8_t y, uint8_t x) {
  uint8_t c;
  if (this->flgCur) {
    c = VPEEK(this->pos_x,this->pos_y);
    this->WRITE(this->pos_x, this->pos_y, c?c:32); 
    this->drawCurs(x, y);  
  }
  this->pos_x = x;
  this->pos_y = y;
}

// シリアルポートスクリーン制御出力(tTVscreenと同じ)
void tGraphicScreen::Serial_Ctrl(int16_t ch) {
  char* s=NULL;
  switch(ch) {
    case KEY_BACKSPACE:
     s = "\x08\x1b[P";
     break;
    case SC_KEY_CTRL_L:
     s = "\x1b[2J\x1b[H";
     break;
  }
  if(s) {
    if(serialMode == 0) {
      while(*s) {
        Serial.write(*s);
        s++;
      }  
    } else if (this->serialMode == 1) {
      while(*s) {
        Serial1.write(*s);
        s++;
      }  
    }  
  }
}
// PS/2キーボードリセット(tTVscreenと同じ)
void tGraphicScreen::reset_kbd(uint8_t kbd_type) {
  endPS2();
  setupPS2(kbd_type);
}

// 文字の出力(tTVscreenと同じ)
void tGraphicScreen::putch(uint8_t c) {
  tscreenBase::putch(c);
 if(serialMode == 0) {
    Serial.write(c);       // シリアル出力
  } else if (this->serialMode == 1) {
    Serial1.write(c);     // シリアル出力  
  }
}

// 改行(tTVscreenと同じ)
void tGraphicScreen::newLine() {  
  tscreenBase::newLine();
  if (this->serialMode == 0) {
    Serial.write(0x0d);
    Serial.write(0x0a);
  } else if (this->serialMode == 1) {
    Serial1.write(0x0d);
    Serial1.write(0x0a);
  }  
}

// キー入力チェック&キーの取得(tTVscreenと同じ)
uint8_t tGraphicScreen::isKeyIn() {
  if(serialMode == 0) {
    if (Serial.available())
      return Serial.read();
  } else if (this->serialMode == 1) {
    if (Serial1.available())
      return Serial1.read();
  }
#if PS2DEV == 1
 return ps2read();
#endif
}

// 文字入力(tTVscreenと同じ)
uint8_t tGraphicScreen::get_ch() {
  char c;
  while(1) {
    if(this->serialMode == 0) {
      if (Serial.available()) {
        c = Serial.read();
        this->dev = 1;
        break;
      }
    } else if (this->serialMode == 1) {
      if (Serial1.available()) {
        c = Serial1.read();
        this->dev = 2;
        break;
      }
    }
#if PS2DEV == 1
    c = ps2read();
    if (c) {
      this->dev = 0;
      break;
    }
#endif
  }
  return c;  
}


// カーソルの表示/非表示
void tGraphicScreen::show_curs(uint8_t flg) {
  this->flgCur = flg;
  if(this->flgCur)
    this->drawCurs(this->pos_x, this->pos_y);
  else 
    this->draw_cls_curs();
}

// カーソルの消去
void tGraphicScreen::draw_cls_curs() {
  uint8_t c = VPEEK(this->pos_x,this->pos_y);
  this->WRITE(this->pos_x, this->pos_y, c?c:32);
}

// スクリーン編集
uint8_t tGraphicScreen::edit() {
  uint8_t ch;  // 入力文字

  do {
    //MOVE(pos_y, pos_x);
    ch = get_ch ();
    show_curs(false);
    if (dev != 1 || allowCtrl) { // USB-Serial(dev=1)は入力コードのキー解釈を行わない
      switch(ch) {
        case KEY_CR:         // [Enter]キー
          show_curs(true);
          return this->enter_text();
          break;
  
        case SC_KEY_CTRL_L:  // [CTRL+L] 画面クリア
          cls();
          locate(0,0);
          Serial_Ctrl(SC_KEY_CTRL_L);
          break;
   
        case KEY_HOME:      // [HOMEキー] 行先頭移動
          locate(0, pos_y);
          break;
          
        case KEY_NPAGE:     // [PageDown] 表示プログラム最終行に移動
          if (pos_x == 0 && pos_y == height-1) {
            this->edit_scrollUp();
          } else {
            this->moveBottom();
          }
          break;
          
        case KEY_PPAGE:     // [PageUP] 画面(0,0)に移動
          if (pos_x == 0 && pos_y == 0) {
            this->edit_scrollDown();
          } else {
            locate(0, 0);
          }  
          break;
  
        case SC_KEY_CTRL_R: // [CTRL_R(F5)] 画面更新
          this->refresh();  break;
  
        case KEY_END:       // [ENDキー] 行の右端移動
           this->moveLineEnd();
           break;
  
        case KEY_IC:         // [Insert]キー
          flgIns = !flgIns;
          break;        
  
        case KEY_BACKSPACE:  // [BS]キー
            this->movePosPrevChar();
            this->delete_char();
           Serial_Ctrl(KEY_BACKSPACE);
          break;        
  
        case KEY_DC:         // [Del]キー
        case SC_KEY_CTRL_X:
          this->delete_char();
          break;        
        
        case KEY_RIGHT:      // [→]キー
          this->movePosNextChar();
          break;
  
        case KEY_LEFT:       // [←]キー
          this->movePosPrevChar();
          break;
  
        case KEY_DOWN:       // [↓]キー
          this->movePosNextLineChar();
          break;
        
        case KEY_UP:         // [↑]キー
          this->movePosPrevLineChar();
          break;
  
        case SC_KEY_CTRL_N:  // 行挿入 
          this->Insert_newLine(pos_y);       
          break;
  
        case SC_KEY_CTRL_D:  // 行削除
          this->clerLine(pos_y);
          break;
        
        default:             // その他
        
        if (IS_PRINT(ch)) {
          this->Insert_char(ch);
        }        
        break;
      }
    } else {
      // PS/2キーボード以外からの入力
      if (ch == KEY_CR) {
        show_curs(true);
        return this->enter_text(); 
      } else {
        this->Insert_char(ch);
      }
    }
    show_curs(true);
  } while(1);
  show_curs(true);
}
