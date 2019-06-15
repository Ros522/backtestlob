// backtest.cpp : アプリケーションのエントリ ポイントを定義します。
//

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

class BackTestEnv {
	struct Order {
		int side;
		float size;
		float price;
	};
private:

	Order position = { -1,0,0 };
	std::vector<Order> orders;

public:
	BackTestEnv() {
	}

	float get_position_side() {
		return this->position.side;
	}
	float get_position_size() {
		return this->position.size;
	}
	float get_position_price() {
		return this->position.price;
	}

	float add_position(Order newpos) {
		float profit = 0;
		if (this->position.side >= 0) {
			if (newpos.side == this->position.side) {
				float sum_size = this->position.size + newpos.size;
				float newprice = (this->position.size * this->position.price + newpos.size*newpos.price) / sum_size;

				Order newposion = { newpos.side, sum_size, newprice };
				this->position = newposion;
			}
			else if (newpos.side == 0 && this->position.side == 1) {
				float after_size = this->position.size - newpos.size;

				float ex_size = this->position.size - std::max(0.0f, after_size);

				profit = (this->position.price - newpos.price)*ex_size;

				if (after_size > 0.0f) {
					Order newposion = { 1,after_size,this->position.price };
					this->position = newposion;
				}
				else if (after_size < 0.0f) {
					Order newposion = { 0,std::abs(after_size),newpos.price };
					this->position = newposion;
				}
				else {
					this->position = { -1,0,0 };
				}
			}
			else if (newpos.side == 1 && this->position.side == 0) {

				float after_size = this->position.size - newpos.size;

				float ex_size = this->position.size - std::max(0.0f, after_size);

				profit = (newpos.price- this->position.price)*ex_size;

				if (after_size > 0.0f) {
					Order newposion = { 0,after_size,this->position.price };
					this->position = newposion;
				}
				else if (after_size < 0.0f) {
					Order newposion = { 1,std::abs(after_size),newpos.price };
					this->position = newposion;
				}
				else {
					this->position = { -1,0,0 };
				}
			}
		}
		else
		{
			this->position = newpos;
		}
		return profit;
	}

	std::tuple<float, int> step(float low, float high) {
		std::vector<Order>::iterator it = this->orders.begin();
		int trade = 0;
		float profit = 0;
		while(it != this->orders.end()) {
			//約定チェック
			Order o = *it;
			if (low < o.price && high > o.price) {
				//約定している
				trade++;
				profit += this->add_position(o);
				it = this->orders.erase(it);
			}
			else
			{
				it++;
			}
		}
		return std::forward_as_tuple(profit, trade);
	}

	std::tuple<float, int> step2(int side, float price) {
		std::vector<Order>::iterator it = this->orders.begin();
		int trade = 0;
		float profit = 0;
		while (it != this->orders.end()) {
			//約定チェック
			Order o = *it;
			//買い履歴の場合、売り注文を見る
			if (side == 0 && o.side == 1 && o.price < price) {
				//約定している
				trade++;
				profit += this->add_position(o);
				it = this->orders.erase(it);
			}
			else if (side == 1 && o.side == 0 && o.price > price) {
			//売り履歴の場合、買い注文を見る
				trade++;
				profit += this->add_position(o);
				it = this->orders.erase(it);
			}
			else
			{
				it++;
			}
		}
		return std::forward_as_tuple(profit, trade);
	}

	void entry(int side, double size, float price) {
		Order o = { side,size,price };
		orders.push_back(o);
	};
	void cancelAll() {
		orders.clear();
	};
};

PYBIND11_MODULE(backtest, m) {
	m.doc() = "backtest_plugin";
	py::class_<BackTestEnv>(m, "BackTestEnv")
		.def(py::init())
		.def("getPositionSide", &BackTestEnv::get_position_side)
		.def("getPositionSize", &BackTestEnv::get_position_size)
		.def("getPositionPrice", &BackTestEnv::get_position_price)
		.def("step", &BackTestEnv::step)
		.def("step2", &BackTestEnv::step2)
		.def("entry", &BackTestEnv::entry)
		.def("cancelAll", &BackTestEnv::cancelAll)
		;
}