import unittest

from backtestlob import BackTestEnv, OrderType, Side


class Simple(unittest.TestCase):
    def test_simplemarket(self):
        env = BackTestEnv()
        # 1枚買い
        orderid = env.entry(OrderType.MARKET, Side.BUY, 1, 0)
        for k, o in env.get_orders().items():
            print(f"Order{k}", o.side, o.size, o.price)

        # Low,Highで約定判定
        profit, trade = env.step(480000, 510000)

        # 510000で約定するはず
        # 本stepでの収支0,ポジション買い　1枚　平均単価51万
        # profit=0,trade=1
        # position=1.0 side=0.0 price=510000.0
        self.assertEqual(profit, 0)
        self.assertEqual(env.size, 1.0)
        self.assertEqual(env.side, Side.BUY)
        self.assertEqual(env.price, 510000)

        # 1枚売り
        orderid = env.entry(OrderType.MARKET, Side.SELL, 1.5, 0)
        for k, o in env.get_orders().items():
            print(f"Order{k}", o.side, o.size, o.price)
        # 約定判定
        profit, trade = env.step(500000, 530000)
        # 500000で約定するはず
        # 本stepでの収支-10000,ポジション売り　0.5枚　平均単価50万
        # profit=-10000,trade=1
        # position=0.5 side=1.0 price=500000.0
        self.assertEqual(profit, -10000)
        self.assertEqual(env.size, 0.5)
        self.assertEqual(env.side, Side.SELL)
        self.assertEqual(env.price, 500000)

    def test_simplemarketbytick(self):
        env = BackTestEnv()
        # 1枚買い
        orderid = env.entry(OrderType.MARKET, Side.BUY, 1, 0)
        for k, o in env.get_orders().items():
            print(f"Order{k}", o.side, o.size, o.price)

        # Low,Highで約定判定
        profit, trade = env.step_by_tick(Side.BUY, 510000)

        # 510000で約定するはず
        # 本stepでの収支0,ポジション買い　1枚　平均単価51万
        # profit=0,trade=1
        # position=1.0 side=0.0 price=510000.0
        self.assertEqual(profit, 0)
        self.assertEqual(env.size, 1.0)
        self.assertEqual(env.side, Side.BUY)
        self.assertEqual(env.price, 510000)

        # 1枚売り
        orderid = env.entry(OrderType.MARKET, Side.SELL, 1.5, 0)
        for k, o in env.get_orders().items():
            print(f"Order{k}", o.side, o.size, o.price)
        # 約定判定
        profit, trade = env.step_by_tick(Side.SELL, 500000)
        # 500000で約定するはず
        # 本stepでの収支-10000,ポジション売り　0.5枚　平均単価50万
        # profit=-10000,trade=1
        # position=0.5 side=1.0 price=500000.0
        self.assertEqual(profit, -10000)
        self.assertEqual(env.size, 0.5)
        self.assertEqual(env.side, Side.SELL)
        self.assertEqual(env.price, 500000)



if __name__ == '__main__':
    unittest.main()
