#ifndef BVH_H
#define BVH_H

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include "Utils/Material.h"
#include "Utils/Triangle.h"

#define INF 114514.0


// BVH ���ڵ�
struct BVHNode {
	int left, right;    // ������������
	int n, index;       // Ҷ�ӽڵ���Ϣ               
	glm::vec3 AA, BB;        // ��ײ��
};


// ������������������ -- �ȽϺ���
bool cmpx(const Triangle& t1, const Triangle& t2) {
	glm::vec3 center1 = (t1.p1 + t1.p2 + t1.p3) / glm::vec3(3, 3, 3);
	glm::vec3 center2 = (t2.p1 + t2.p2 + t2.p3) / glm::vec3(3, 3, 3);
	return center1.x < center2.x;
}
bool cmpy(const Triangle& t1, const Triangle& t2) {
	glm::vec3 center1 = (t1.p1 + t1.p2 + t1.p3) / glm::vec3(3, 3, 3);
	glm::vec3 center2 = (t2.p1 + t2.p2 + t2.p3) / glm::vec3(3, 3, 3);
	return center1.y < center2.y;
}
bool cmpz(const Triangle& t1, const Triangle& t2) {
	glm::vec3 center1 = (t1.p1 + t1.p2 + t1.p3) / glm::vec3(3, 3, 3);
	glm::vec3 center2 = (t2.p1 + t2.p2 + t2.p3) / glm::vec3(3, 3, 3);
	return center1.z < center2.z;
}

// SAH �Ż����� BVH
int buildBVHwithSAH(std::vector<Triangle>& triangles, std::vector<BVHNode>& nodes, int l, int r, int n) {
	if (l > r) return 0;

	nodes.push_back(BVHNode());
	int id = nodes.size() - 1;
	nodes[id].left = nodes[id].right = nodes[id].n = nodes[id].index = 0;
	nodes[id].AA = glm::vec3(1145141919, 1145141919, 1145141919);
	nodes[id].BB = glm::vec3(-1145141919, -1145141919, -1145141919);

	// ���� AABB
	for (int i = l; i <= r; i++) {
		// ��С�� AA
		float minx = glm::min(triangles[i].p1.x, glm::min(triangles[i].p2.x, triangles[i].p3.x));
		float miny = glm::min(triangles[i].p1.y, glm::min(triangles[i].p2.y, triangles[i].p3.y));
		float minz = glm::min(triangles[i].p1.z, glm::min(triangles[i].p2.z, triangles[i].p3.z));
		nodes[id].AA.x = glm::min(nodes[id].AA.x, minx);
		nodes[id].AA.y = glm::min(nodes[id].AA.y, miny);
		nodes[id].AA.z = glm::min(nodes[id].AA.z, minz);
		// ���� BB
		float maxx = glm::max(triangles[i].p1.x, glm::max(triangles[i].p2.x, triangles[i].p3.x));
		float maxy = glm::max(triangles[i].p1.y, glm::max(triangles[i].p2.y, triangles[i].p3.y));
		float maxz = glm::max(triangles[i].p1.z, glm::max(triangles[i].p2.z, triangles[i].p3.z));
		nodes[id].BB.x = glm::max(nodes[id].BB.x, maxx);
		nodes[id].BB.y = glm::max(nodes[id].BB.y, maxy);
		nodes[id].BB.z = glm::max(nodes[id].BB.z, maxz);
	}

	// ������ n �������� ����Ҷ�ӽڵ�
	if ((r - l + 1) <= n) {
		nodes[id].n = r - l + 1;
		nodes[id].index = l;
		return id;
	}

	// ����ݹ齨��
	float Cost = INF;
	int Axis = 0;
	int Split = (l + r) / 2;
	for (int axis = 0; axis < 3; axis++) {
		// �ֱ� x��y��z ������
		if (axis == 0) std::sort(&triangles[0] + l, &triangles[0] + r + 1, cmpx);
		if (axis == 1) std::sort(&triangles[0] + l, &triangles[0] + r + 1, cmpy);
		if (axis == 2) std::sort(&triangles[0] + l, &triangles[0] + r + 1, cmpz);

		// leftMax[i]: [l, i] ������ xyz ֵ
		// leftMin[i]: [l, i] ����С�� xyz ֵ
		std::vector<glm::vec3> leftMax(r - l + 1, glm::vec3(-INF, -INF, -INF));
		std::vector<glm::vec3> leftMin(r - l + 1, glm::vec3(INF, INF, INF));
		// ����ǰ׺ ע�� i-l �Զ��뵽�±� 0
		for (int i = l; i <= r; i++) {
			Triangle& t = triangles[i];
			int bias = (i == l) ? 0 : 1;  // ��һ��Ԫ�����⴦��

			leftMax[i - l].x = glm::max(leftMax[i - l - bias].x, glm::max(t.p1.x, glm::max(t.p2.x, t.p3.x)));
			leftMax[i - l].y = glm::max(leftMax[i - l - bias].y, glm::max(t.p1.y, glm::max(t.p2.y, t.p3.y)));
			leftMax[i - l].z = glm::max(leftMax[i - l - bias].z, glm::max(t.p1.z, glm::max(t.p2.z, t.p3.z)));

			leftMin[i - l].x = glm::min(leftMin[i - l - bias].x, glm::min(t.p1.x, glm::min(t.p2.x, t.p3.x)));
			leftMin[i - l].y = glm::min(leftMin[i - l - bias].y, glm::min(t.p1.y, glm::min(t.p2.y, t.p3.y)));
			leftMin[i - l].z = glm::min(leftMin[i - l - bias].z, glm::min(t.p1.z, glm::min(t.p2.z, t.p3.z)));
		}

		// rightMax[i]: [i, r] ������ xyz ֵ
		// rightMin[i]: [i, r] ����С�� xyz ֵ
		std::vector<glm::vec3> rightMax(r - l + 1, glm::vec3(-INF, -INF, -INF));
		std::vector<glm::vec3> rightMin(r - l + 1, glm::vec3(INF, INF, INF));
		// �����׺ ע�� i-l �Զ��뵽�±� 0
		for (int i = r; i >= l; i--) {
			Triangle& t = triangles[i];
			int bias = (i == r) ? 0 : 1;  // ��һ��Ԫ�����⴦��

			rightMax[i - l].x = glm::max(rightMax[i - l + bias].x, glm::max(t.p1.x, glm::max(t.p2.x, t.p3.x)));
			rightMax[i - l].y = glm::max(rightMax[i - l + bias].y, glm::max(t.p1.y, glm::max(t.p2.y, t.p3.y)));
			rightMax[i - l].z = glm::max(rightMax[i - l + bias].z, glm::max(t.p1.z, glm::max(t.p2.z, t.p3.z)));

			rightMin[i - l].x = glm::min(rightMin[i - l + bias].x, glm::min(t.p1.x, glm::min(t.p2.x, t.p3.x)));
			rightMin[i - l].y = glm::min(rightMin[i - l + bias].y, glm::min(t.p1.y, glm::min(t.p2.y, t.p3.y)));
			rightMin[i - l].z = glm::min(rightMin[i - l + bias].z, glm::min(t.p1.z, glm::min(t.p2.z, t.p3.z)));
		}

		// ����Ѱ�ҷָ�
		float cost = INF;
		int split = l;
		for (int i = l; i <= r - 1; i++) {
			float lenx, leny, lenz;
			// ��� [l, i]
			glm::vec3 leftAA = leftMin[i - l];
			glm::vec3 leftBB = leftMax[i - l];
			lenx = leftBB.x - leftAA.x;
			leny = leftBB.y - leftAA.y;
			lenz = leftBB.z - leftAA.z;
			float leftS = 2.0 * ((lenx * leny) + (lenx * lenz) + (leny * lenz));
			float leftCost = leftS * (i - l + 1);

			// �Ҳ� [i+1, r]
			glm::vec3 rightAA = rightMin[i + 1 - l];
			glm::vec3 rightBB = rightMax[i + 1 - l];
			lenx = rightBB.x - rightAA.x;
			leny = rightBB.y - rightAA.y;
			lenz = rightBB.z - rightAA.z;
			float rightS = 2.0 * ((lenx * leny) + (lenx * lenz) + (leny * lenz));
			float rightCost = rightS * (r - i);

			// ��¼ÿ���ָ����С��
			float totalCost = leftCost + rightCost;
			if (totalCost < cost) {
				cost = totalCost;
				split = i;
			}
		}
		// ��¼ÿ�������Ѵ�
		if (cost < Cost) {
			Cost = cost;
			Axis = axis;
			Split = split;
		}
	}

	// �������ָ�
	if (Axis == 0) std::sort(&triangles[0] + l, &triangles[0] + r + 1, cmpx);
	if (Axis == 1) std::sort(&triangles[0] + l, &triangles[0] + r + 1, cmpy);
	if (Axis == 2) std::sort(&triangles[0] + l, &triangles[0] + r + 1, cmpz);

	// �ݹ�
	int left = buildBVHwithSAH(triangles, nodes, l, Split, n);
	int right = buildBVHwithSAH(triangles, nodes, Split + 1, r, n);

	nodes[id].left = left;
	nodes[id].right = right;

	return id;
}



#endif
