#ifndef _RollingLeastSquares_h_
#define _RollingLeastSquares_h_
#include <algorithm>
/*
 * Tips for expanding this into a general running quadratic or linear least squares regression 
 * Based on 
 * http://mathforum.org/library/drmath/view/72047.html
 * 
 *  
 * maintain sums such that  
 * Sab  is sum of X^a*Y^b for all each x and y in the data set. 
 * 
 * so when adding a point x,y with weight w
 * 
 *  S00 += 1 * w;
 *  S10 += x * w;
 *  S20 += x * x * w;
 *  S30 += x * x * x * w;
 *  S40 += x * x * x * x * w;
 *  S01 += y * w;
 *  S11 += x * y * w;
 *  S21 += x * x * y * w;
 *  S31 += x * x * x * y * w;
 *  S41 += x * x * x * x * y * w;
 * 
 *  Then the parameters a,b,c of the best fit equation y = ax^2 + bx + c
 *  are given by 
 *  
  a = (S01*S10*S30 - S11*S00*S30 - S01*S20*S20
       + S11*S10*S20 + S21*S00*S20 - S21*S10*S10)
    /(S00*S20*S40 - S10*S10*S40 - S00*S30*S30 + 2*S10*S20*S30 - S20*S20*S20)

  b = (S11*S00*S40 - S01*S10*S40 + S01*S20*S30
       - S21*S00*S30 - S11*S20*S20 + S21*S10*S20)
    /(S00*S20*S40 - S10*S10*S40 - S00*S30*S30 + 2*S10*S20*S30 - S20*S20*S20)

  c = (S01*S20*S40 - S11*S10*S40 - S01*S30*S30
       + S11*S20*S30 + S21*S10*S30 - S21*S20*S20)
    /(S00*S20*S40 - S10*S10*S40 - S00*S30*S30 + 2*S10*S20*S30 - S20*S20*S20)
 
  
  and the parameters m and b of the best fit y = mx + b are given by 
  
  m = (S00 * S11 - S10 * S01) / (S00 * S20 - S10 * S10)
  b = (S01 - m * S10) / S00 
  
  and of course the average is given by 
  
  y = S01 / S00  (sum of y's divided by count)
  
  variance and sum of errors could probably also be computed from these sums, dunno
  
 */

template <class T, class SumT> 
class RollingLeastSquaresBase {
    public:
    T *xs, *ys, *weights;
    int index = 0, size = 0;
    int totalWeight = 0;
    SumT X = 0, Y = 0, XX = 0, XY = 0, xBase = 0;
    int count = 0;
    void clear() { X = Y = XX = XY = 0; count = 0; } 
    
    RollingLeastSquaresBase(int s, T *x, T *y, T*w) : size(s), xs(x), ys(y), weights(w)  {}
    void add(T x, T y) { 
    	add(x, y, 1.0);
    }
    T slope() {
        if (count < 2) return 0.0;
        return (totalWeight * XY - X * Y) / (totalWeight * XX - X * X);
	}	
	T intercept() { 
		if (count < 1) return 0.0;
		T b = (Y - slope() * X) / (totalWeight);
		return slope() * -xBase + b;
	}
	
    T predict(T x) { 
		//x -= xBase;
    	return slope() * x + intercept();
	}
    T err(T x, T y) {
		x -= xBase;
		return std::abs((-x * slope()) + y - intercept()) / 
                        sqrt(slope() * slope() + 1);
	}
    T rmsError() {
        T s = 0;
        for (int i = 0; i < count; i++) {
            int idx = (index + size - count + i) % size;
            T e = err(xs[idx], ys[idx]);
            s += e * e;
        }
        return sqrt(s / count);
    }
    
    void rebaseX() {
		if (count < size)
			return;
		
		T delta = xs[0] - xBase;
		xBase = xs[0];
		X = Y = XX = XY = totalWeight = 0;
		for(int n = 0; n < count; n++) {
			xs[n] -= delta;
            X += xs[n] * weights[n];
            Y += ys[n] * weights[n];
            XX += xs[n] * xs[n] * weights[n];
            XY += xs[n] * ys[n] * weights[n];
            totalWeight += weights[n];
		}
	}
    T averageY() {
    	return totalWeight > 0 ? Y / totalWeight : 0;
    }
    void add(T x, T y, T w) {
		x -= xBase;
        if (count == size) {
            X -= xs[index] * weights[index];
            Y -= ys[index] * weights[index];
            XX -= xs[index] * xs[index] * weights[index];
            XY -= xs[index] * ys[index] * weights[index];
            totalWeight -= weights[index];
        }
        
        weights[index] = w;
        xs[index] = x;
        ys[index++] = y;
        
        if (index >= size) {
            index = 0;
        }
        if (count < size) {
            count++;
        }

        X += x * w;
        Y += y * w;
        XX += x * x * w;
        XY += x * y * w;
        totalWeight += w;
    }
	bool full() { return count == size; }

    void reset() {
        count = index = xBase = 0;
        X = Y = XX = XY = totalWeight = 0.0;
    }
    void removeLast() {
        count--;
        index--;
        if (index < 0) {
            index = size - 1;
        }
        X -= xs[index] * weights[index];
        Y -= ys[index] * weights[index];
        XX -= xs[index] * xs[index] * weights[index];
        XY -= xs[index] * ys[index] * weights[index];
        totalWeight -= weights[index];
    }
};

template <class T, class SumT, int SIZE> 
class RollingLeastSquaresStatic : public RollingLeastSquaresBase<T, SumT> { 
    T xs[SIZE], ys[SIZE], weights[SIZE];
public:
	RollingLeastSquaresStatic() : RollingLeastSquaresBase<T, SumT>(SIZE, xs, ys, weights) {}
};

template <class T, class SumT> 
class RollingLeastSquaresDynamic : public RollingLeastSquaresBase<T, SumT> { 
public:
	RollingLeastSquaresDynamic(int s) : RollingLeastSquaresBase<T, SumT>(s, new T[s], new T[s], new T[s]) {}
	~RollingLeastSquaresDynamic() { 
		delete RollingLeastSquaresBase<T, SumT>::xs; 
		delete RollingLeastSquaresBase<T, SumT>::ys; 
		delete RollingLeastSquaresBase<T, SumT>::weights; }
};
	
		
typedef RollingLeastSquaresDynamic<double,double> RollingLeastSquares;


template <class T>
class TwoStageRollingLeastSquares { 
public:
	RollingLeastSquaresDynamic<T,T> stage1, stage2;
	int idx = 0;
	TwoStageRollingLeastSquares(int s1, int s2) : stage1(s1), stage2(s2) {}
	void add(T x, T y, T w = 1.0) { 
		stage1.add(x, y, w);
		idx++;
		if (idx == stage1.size) { 
			idx = 0;
			stage2.add(x, stage1.averageY());
		}
	}  
	T average() { return stage2.averageY(); } 
	T slope() { return stage2.slope(); } 
	bool full() { return stage2.count == stage2.size; }
	void reset() { stage1.reset(); stage2.reset(); idx = 0; }  
};

template <class T, int SIZE>
class RollingAverage {
	T values[SIZE];
	float sum = 0;
public:
	int count = 0;
	int index = 0;
	RollingAverage() {}
	void reset() { sum = count = index = 0; } 
	void add(const T &v) { 
		if (count == SIZE) 
			sum -= values[index];
		else 
			count++;
		values[index++] = v;
		sum += v;
		if (index >= SIZE)
			index = 0;
	}
	float average() { 
		return count > 0 ? ((float)sum)/count : 0;
	}
	T min() { 
		T m = values[0];
		for (int n = 0; n < count; n++)
			if (values[n] < m) m = values[n];
		return m;
	}
	T max() { 
		T m = values[0];
		for (int n = 0; n < count; n++)
			if (values[n] > m) m = values[n];
		return m;
	}
	bool full() { return count == SIZE; } 
};

template <class T, int SIZE1, int SIZE2>
class TwoStageRollingAverage {
public:
	RollingAverage<T,SIZE1> stage1;
	RollingAverage<T,SIZE2> stage2;
	T maxs[SIZE2] = {0}, mins[SIZE2] = {0};

	TwoStageRollingAverage() {}
	void add(const T &v) {
		stage1.add(v);
		if (stage1.count == SIZE1) { 
			maxs[stage2.index] = stage1.max();
			mins[stage2.index] = stage1.min();
			stage2.add(stage1.average());
			stage1.reset();
		}
	}
	T min() { 
		T m = mins[0];
		for(int n = 1; n < stage2.count; n++) 
			m = std::min(mins[n], m);
		return stage1.count != 0 ? std::min(stage1.min(), m) : m;
	}
	T max() { 
		T m = maxs[0];
		for(int n = 1; n < stage2.count; n++) 
			m = std::max(maxs[n], m);
		return stage1.count != 0 ? std::max(stage1.max(), m) : m;
	}
	double average() {
		if (stage1.count + stage2.count == 0) 
			return 0;
		return (stage2.average() * stage2.count * SIZE1 + stage1.average() * stage1.count) /
			(stage2.count * SIZE1 + stage1.count);
	}
	bool full() { return stage2.full(); } 
};

#endif //_RollingLeastSquares_h_
