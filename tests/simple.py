import unittest

from backtestlob import BackTestEnv, OrderType, Side


class Simple(unittest.TestCase):
    def test_simple(self):
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


if __name__ == '__main__':
    unittest.main()
