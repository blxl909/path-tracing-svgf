#ifndef HDR_COMPUTE_H
#define HDR_COMPUTE_H

// ���� HDR ��ͼ��ػ�����Ϣ
float* calculateHdrCache(float* HDR, int width, int height) {

	float lumSum = 0.0;

	// ��ʼ�� h �� w �еĸ����ܶ� pdf �� ͳ��������
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

	// �����ܶȹ�һ��
	for (int i = 0; i < height; i++)
		for (int j = 0; j < width; j++)
			pdf[i][j] /= lumSum;

	// �ۼ�ÿһ�еõ� x �ı�Ե�����ܶ�
	std::vector<float> pdf_x_margin;
	pdf_x_margin.resize(width);
	for (int j = 0; j < width; j++)
		for (int i = 0; i < height; i++)
			pdf_x_margin[j] += pdf[i][j];

	// ���� x �ı�Ե�ֲ�����
	std::vector<float> cdf_x_margin = pdf_x_margin;
	for (int i = 1; i < width; i++)
		cdf_x_margin[i] += cdf_x_margin[i - 1];

	// ���� y �� X=x �µ����������ܶȺ���
	std::vector<std::vector<float>> pdf_y_condiciton = pdf;
	for (int j = 0; j < width; j++)
		for (int i = 0; i < height; i++)
			pdf_y_condiciton[i][j] /= pdf_x_margin[j];

	// ���� y �� X=x �µ��������ʷֲ�����
	std::vector<std::vector<float>> cdf_y_condiciton = pdf_y_condiciton;
	for (int j = 0; j < width; j++)
		for (int i = 1; i < height; i++)
			cdf_y_condiciton[i][j] += cdf_y_condiciton[i - 1][j];

	// cdf_y_condiciton ת��Ϊ���д洢
	// cdf_y_condiciton[i] ��ʾ y �� X=i �µ��������ʷֲ�����
	std::vector<std::vector<float>> temp = cdf_y_condiciton;
	cdf_y_condiciton = std::vector<std::vector<float>>(width);
	for (auto& line : cdf_y_condiciton) line.resize(height);
	for (int j = 0; j < width; j++)
		for (int i = 0; i < height; i++)
			cdf_y_condiciton[j][i] = temp[i][j];

	// ��� xi_1, xi_2 Ԥ�������� xy
	// sample_x[i][j] ��ʾ xi_1=i/height, xi_2=j/width ʱ (x,y) �е� x
	// sample_y[i][j] ��ʾ xi_1=i/height, xi_2=j/width ʱ (x,y) �е� y
	// sample_p[i][j] ��ʾȡ (i, j) ��ʱ�ĸ����ܶ�
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

			// �� xi_1 �� cdf_x_margin �� lower bound �õ����� x
			int x = std::lower_bound(cdf_x_margin.begin(), cdf_x_margin.end(), xi_1) - cdf_x_margin.begin();
			// �� xi_2 �� X=x ������µõ����� y
			int y = std::lower_bound(cdf_y_condiciton[x].begin(), cdf_y_condiciton[x].end(), xi_2) - cdf_y_condiciton[x].begin();

			// �洢�������� xy �� xy λ�ö�Ӧ�ĸ����ܶ�
			sample_x[i][j] = float(x) / width;
			sample_y[i][j] = float(y) / height;
			sample_p[i][j] = pdf[i][j];
		}
	}

	// ���Ͻ��������
	// R,G ͨ���洢���� (x,y) �� B ͨ���洢 pdf(i, j)
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
