# backtestlob
Limit Order Book 指値戦略の高速バックテスト
(on pybind11)
# 機能
1. 買い指値に対して、「買い約定履歴 < 指値」　で注文の約定判定を行う 
2. 売り指値に対して、「売り約定履歴 < 指値」　で注文の約定判定を行う 
3. 約定したら内部のポジションを更新して、その注文約定の収支を返す
4. 注文はネットアウト注文として保持する
5. 安値、高値入力で約定判定(step)、または1約定履歴流し込み(step2)
6. 成行できない

# イメージ
```
from backtest import BackTestEnv
env = BackTestEnv()
#1枚買い
env.entry(0,1,500000)
#Low,Highで約定判定
profit,trade = env.step(480000,510000)
print(f"profit={profit},trade={trade}")
print(f"position={env.getPositionSize()} side={env.getPositionSide()} price={env.getPositionPrice()}")

#本stepでの収支0,ポジション買い　1枚　平均単価50万
#profit=0,trade=1
#position=1.0 side=0.0 price=500000.0

#1枚売り
env.entry(1,1.5,520000)
#約定判定
profit,trade = env.step(510000,530000)

print(f"profit={profit},trade={trade}")
print(f"position={env.getPositionSize()} side={env.getPositionSide()} price={env.getPositionPrice()}")

#本stepでの収支20000,ポジション売り　0.5枚　平均単価52万
#profit=20000,trade=1
#position=0.5 side=1.0 price=520000.0
```