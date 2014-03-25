//以下のパラメータを手動で変えること
float PParameter = 0.1; //PID制御のPパラメータ
float DParameter = 1; //PID制御のDパラメータ
float AimCO2Rate = 400; // 目標CO2濃度[ppm]
float AllowanceRate = 50; // 目標CO2濃度からどの程度はなれてもよいか[ppm]
int measureInterval = 100; // 測定インターバル時間(ミリ秒)
boolean isTestOn = false; // テストをするかどうか

// 以下は変更しないこと
float previousCO2Rate; // 最後の測定の前のCO2濃度
float nowCO2Rate; // 現在のCO2濃度
float dTRatiodiv = 0; // PID制御によるDT比変化量
float dTRatio = 0; // DT比
int measureCount = 0; // 最後にDT比を変えてからCO2濃度を測定した回数
int changeInterval = 100; // DT比を変える測定までの測定回数(最低値:100 最高値:2560)
int openTimes = 0; // CO2濃度を何回測定するまで電子弁をあけるか
int switchcounter = 0; // 電磁弁が何回ONされたか(テスト用）
boolean outCO2RangeFlag = false; // CO2濃度が指定した範囲外となったか
boolean inCO2RangeFlag = false; // CO2濃度が指定した範囲内となったか
boolean electricValveStatus = false; // 電子弁の状態（false:閉め　true:空き）
boolean inputAlart = false; // 入力されたCO2濃度データが正常か（false:正常　true:異常）

void setup(){  
  // 初期設定
  Serial.begin(9600);
  pinMode(13,OUTPUT);
  digitalWrite(13,LOW);
  
  // CO2濃度を読み込む
  previousCO2Rate = measureCO2Rate();
}

void loop(){ 
  // 測定回数が一定値を超えたらDT比をチェンジする
  if(measureCount >= changeInterval){
    // CO2濃度が安定しているかによってDT比を変える測定回数を変える
    if(outCO2RangeFlag == true){
      changeInterval = changeInterval / 2;
      // 境界値を超えた場合は戻す
      if(changeInterval < 100){
        changeInterval = 100;
      }
    }else if(inCO2RangeFlag == true){
      changeInterval = changeInterval * 2;
      // 境界値を超えた場合は戻す
      if(changeInterval > 2570){
        changeInterval = 2560;
      }
    }
    
    // CO2濃度が一度も指定した範囲内になかった場合は最低値に戻す
    if(inCO2RangeFlag == false){
      changeInterval = 100;
    }
    
    // 各計測によるDT比の変更量を平均するために測定回数で割る（100で割っているのはなんとなくなので消しても良い）
    dTRatiodiv = dTRatiodiv / measureCount / 100;
    
    // PID制御によるDT比の変更(境界値を超えた場合は境界値に戻す）
    dTRatio = dTRatio + dTRatiodiv;
    if(dTRatio < 0){
      dTRatio = 0;
    }else if(dTRatio > 1.0){
      dTRatio = 1;
    }
    
    // DT比に従って電子弁を開ける時間を決める（openTimesだけCO2濃度を測定したら電子弁を閉める）
    openTimes = changeInterval * dTRatio;
    
    // DT比を変えたら各値をリセットする
    measureCount = 0;
    dTRatiodiv = 0;
    inCO2RangeFlag = false;
    outCO2RangeFlag = false;
  }
  
  // 電磁弁をDT比に従って開け閉めし、現在の電子弁のステータスを更新する
  switchElectricValve();
  
  // 前回のCO2濃度を保持し、現在のCO2濃度を計測する
  previousCO2Rate = nowCO2Rate;
  nowCO2Rate = measureCO2Rate();
  measureCount++;
  
  // 不正なデータならnowCO2Rateは0となるがfloat型を同値で判断するのは危険なので1より小さいで判断
  if(nowCO2Rate < 1){
    inputAlart = true;
  }
  
  // PID制御(実際はPD制御)の制御値を計算する（ミリ秒を秒に直すため1000をかけている）
  // 後で平均するために足しておく
  dTRatiodiv = dTRatiodiv + PParameter * (AimCO2Rate - nowCO2Rate) - DParameter * (nowCO2Rate - previousCO2Rate) / measureInterval * 1000;
  
  // 現在のCO2濃度が指定した範囲内に入っているかチェック
  if(AimCO2Rate - AllowanceRate < nowCO2Rate && nowCO2Rate < AimCO2Rate + AllowanceRate){
    inCO2RangeFlag = true;
  }else{
    outCO2RangeFlag = true;
  }

  // テスト用データ取得
  returnTestData();
  
  delay(measureInterval);
}

float measureCO2Rate(){
  float CO2Rate = 0;
  float voltage = analogRead(0) * 2.0 /1024 * 5.0; // ロガーの電圧値を2分の１にして入力信号としているため2倍して元に戻す
  // 入力電圧が低すぎる場合は不正な入力データとして0を返す
  if(voltage > 1){
    CO2Rate = -1.3721 * (voltage * voltage * voltage) + 38.984 * (voltage * voltage) - 76.703 * voltage + 5.065;
  }
  return CO2Rate;
}

void switchElectricValve(){
  // 入力信号が不正なデータの場合は強制的に電子弁を閉じる
  if(inputAlart == false){
    if(measureCount < openTimes && electricValveStatus == false){
      digitalWrite(13,HIGH);
      electricValveStatus = true;
      switchcounter++;
    }else if(measureCount > openTimes && electricValveStatus == true){
      digitalWrite(13,LOW);
      electricValveStatus = false;
    }
  }else{
    digitalWrite(13,LOW);
    electricValveStatus = false;
  }
}

void returnTestData(){
  if (Serial.available() > 0 && isTestOn){
    int incomingByte = Serial.read();
    Serial.println(switchcounter);
    Serial.println(nowCO2Rate);
    Serial.println(changeInterval);
    Serial.flush();
  }
}
