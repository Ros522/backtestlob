# backtestlob
Limit Order Book 指値戦略の高速バックテスト
(on pybind11)
```
pip3 install git+https://github.com/Ros522/backtestlob
```

# 将来的な機能
1. 成行のシミュレーション  
※現行はlow-highの場合、常に不利価格で約定すると判定（売り成行はHIGH価格で約定）  
　bytickの場合、次にきた同サイドの注文の価格にて約定すると判定
1. 取引所遅延のシミュレーション
https://github.com/Ros522/lazy_backtest_lob
1. 取引所APIラッパー
1. ビジュアライゼーション（by Python）

# 機能
1. 買い指値に対して、「買い約定履歴 < 指値」　で注文の約定判定を行う 
2. 売り指値に対して、「売り約定履歴 > 指値」　で注文の約定判定を行う 
3. 約定したら内部のポジションを更新して、その注文約定の収支、トレード回数返す
4. 注文はネットアウト注文として保持する
5. 安値、高値入力で約定判定(step)、または1約定履歴流し込み(step_by_tick)
6. 成行は試験中

## exported class
* OrderType
  - OrderType.LIMIT
  - OrderType.MARKET
  
* Side
  - Side.BUY
  - Side.SELL
  
* BackTestEnv
    ### property
    - size  
    現在の建玉のサイズを示す
    - side  
    現在の建玉が、Side.BUYかSide.SELLかを示す
    - price  
    現在の建玉の平均価格を示す
    ### func
    - get_orders
    ```
    for k, o in env.get_orders().items():
        print(f"Order{k}", o.side, o.size, o.price)
    ```
    - entry 
    ```
    orderid = env.entry(OrderType, Side, size, price)
    ``` 
    - cancel
    ```
    env.cancel(orderid)
    ```
    - cancel_all
    ```
    env.cancel_all()
    ```
      

# イメージ
```
env = BackTestEnv()
# 1枚買い
orderid = env.entry(OrderType.LIMIT, Side.BUY, 1, 500000)
for k, o in env.get_orders().items():
    print(f"Order{k}", o.side, o.size, o.price)

# Low,Highで約定判定
profit, trade = env.step(480000, 510000)

# 本stepでの収支0,ポジション買い　1枚　平均単価50万
# profit=0,trade=1
# position=1.0 side=0.0 price=500000.0
self.assertEqual(profit, 0)
self.assertEqual(env.size, 1.0)
self.assertEqual(env.side, Side.BUY)
self.assertEqual(env.price, 500000)

# 1枚売り
orderid = env.entry(OrderType.LIMIT, Side.SELL, 1.5, 520000)
for k, o in env.get_orders().items():
    print(f"Order{k}", o.side, o.size, o.price)
# 約定判定
profit, trade = env.step(510000, 530000)

# 本stepでの収支20000,ポジション売り　0.5枚　平均単価52万
# profit=20000,trade=1
# position=0.5 side=1.0 price=520000.0
self.assertEqual(profit, 20000)
self.assertEqual(env.size, 0.5)
self.assertEqual(env.side, Side.SELL)
self.assertEqual(env.price, 520000)
```
