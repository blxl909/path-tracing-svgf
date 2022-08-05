#ifndef HDR_COMPUTE_H
#define HDR_COMPUTE_H

// 计算 HDR 贴图相关缓存信息
float* calculateHdrCache(float* HDR, int width, int height) {

	float lumSum = 0.0;

	// 初始化 h 行 w 列的概率密度 pdf 并 统计总亮度
	std::vector<std::vector<float>> pdf(height);
	for (auto& line : pdf) line.resize(width);
	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			float R = HDR[3 * (i * width + j)];
			float G = HDR[3 * (i * width + j) + 1];
			float B = HDR[3 * (i * width + j) + 2];
			float lum = 0.2 * R + 0.7 * G + 0.1 * B;
			pdf[i][j] = lum;
			lumSum += lum;
		}
	}

	// 概率密度归一化
	for (int i = 0; i < height; i++)
		for (int j = 0; j < width; j++)
			pdf[i][j] /= lumSum;

	// 累加每一列得到 x 的边缘概率密度
	std::vector<float> pdf_x_margin;
	pdf_x_margin.resize(width);
	for (int j = 0; j < width; j++)
		for (int i = 0; i < height; i++)
			pdf_x_margin[j] += pdf[i][j];

	// 计算 x 的边缘分布函数
	std::vector<float> cdf_x_margin = pdf_x_margin;
	for (int i = 1; i < width; i++)
		cdf_x_margin[i] += cdf_x_margin[i - 1];

	// 计算 y 在 X=x 下的条件概率密度函数
	std::vector<std::vector<float>> pdf_y_condiciton = pdf;
	for (int j = 0; j < width; j++)
		for (int i = 0; i < height; i++)
			pdf_y_condiciton[i][j] /= pdf_x_margin[j];

	// 计算 y 在 X=x 下的条件概率分布函数
	std::vector<std::vector<float>> cdf_y_condiciton = pdf_y_condiciton;
	for (int j = 0; j < width; j++)
		for (int i = 1; i < height; i++)
			cdf_y_condiciton[i][j] += cdf_y_condiciton[i - 1][j];

	// cdf_y_condiciton 转置为按列存储
	// cdf_y_condiciton[i] 表示 y 在 X=i 下的条件概率分布函数
	std::vector<std::vector<float>> temp = cdf_y_condiciton;
	cdf_y_condiciton = std::vector<std::vector<float>>(width);
	for (auto& line : cdf_y_condiciton) line.resize(height);
	for (int j = 0; j < width; j++)
		for (int i = 0; i < height; i++)
			cdf_y_condiciton[j][i] = temp[i][j];

	// 穷举 xi_1, xi_2 预计算样本 xy
	// sample_x[i][j] 表示 xi_1=i/height, xi_2=j/width 时 (x,y) 中的 x
	// sample_y[i][j] 表示 xi_1=i/height, xi_2=j/width 时 (x,y) 中的 y
	// sample_p[i][j] 表示取 (i, j) 点时的概率密度
	std::vector<std::vector<float>> sample_x(height);
	for (auto& line : sample_x) line.resize(width);
	std::vector<std::vector<float>> sample_y(height);
	for (auto& line : sample_y) line.resize(width);
	std::vector<std::vector<float>> sample_p(height);
	for (auto& line : sample_p) line.resize(width);
	for (int j = 0; j < width; j++) {
		for (int i = 0; i < height; i++) {
			float xi_1 = float(i) / height;
			float xi_2 = float(j) / width;

			// 用 xi_1 在 cdf_x_margin 中 lower bound 得到样本 x
			int x = std::lower_bound(cdf_x_margin.begin(), cdf_x_margin.end(), xi_1) - cdf_x_margin.begin();
			// 用 xi_2 在 X=x 的情况下得到样本 y
			int y = std::lower_bound(cdf_y_condiciton[x].begin(), cdf_y_condiciton[x].end(), xi_2) - cdf_y_condiciton[x].begin();

			// 存储纹理坐标 xy 和 xy 位置对应的概率密度
			sample_x[i][j] = float(x) / width;
			sample_y[i][j] = float(y) / height;
			sample_p[i][j] = pdf[i][j];
		}
	}

	// 整合结果到纹理
	// R,G 通道存储样本 (x,y) 而 B 通道存储 pdf(i, j)
	float* cache = new float[width * height * 3];
	//for (int i = 0; i < width * height * 3; i++) cache[i] = 0.0;

	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			cache[3 * (i * width + j)] = sample_x[i][j];        // R
			cache[3 * (i * width + j) + 1] = sample_y[i][j];    // G
			cache[3 * (i * width + j) + 2] = sample_p[i][j];    // B
		}
	}

	return cache;
}

#endif
