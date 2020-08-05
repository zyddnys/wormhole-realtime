#pragma once

#include "common.h"

#include "Wormhole.h"

struct Camera
{
public:
	Camera() :
		m_right(1.f, 0.f, 0.f),
		m_up(0.f, 1.f, 0.f),
		m_look(0.f, 0.f, 1.f),
		m_position(2.f, 2.f, 2.f),
		m_fovY(0.78539816299999998f),
		m_nearZ(0.1f),
		m_farZ(100.0f),
		m_speed(10.0f),
		m_rotateRate(1.0f),
		width(1280),
		height(720),
		l(1.0f)
	{

	}
	~Camera()
	{

	}
public:

	void SetPosition(float x, float y, float z) { m_position = XMFLOAT3(x, y, z); }
	void SetPositionXM(FXMVECTOR pos) { XMStoreFloat3(&m_position, pos); }

	//�������λ�ü�������ز���
	XMFLOAT3 GetPosition()	const { return m_position; }
	XMFLOAT3 GetRight()		const { return m_right; }
	XMFLOAT3 GetUp()		const { return m_up; }
	XMFLOAT3 GetLookDir()	const { return m_look; }
	XMFLOAT3 GetLookAt()	const {
		XMFLOAT3 tmp;
		XMStoreFloat3(&tmp, GetLookAtM());
		return tmp;
	};

	XMVECTOR GetPositionXM()	const { return XMLoadFloat3(&m_position); }
	XMVECTOR GetRightXM()		const { return XMLoadFloat3(&m_right); }
	XMVECTOR GetUpXM()			const { return XMLoadFloat3(&m_up); }
	XMVECTOR GetLookDirXM()		const { return XMLoadFloat3(&m_look); }
	XMVECTOR GetLookAtM()		const { return GetPositionXM() + GetLookDirXM(); };

	//���ͶӰ��ز���
	float GetNearZ()	const { return m_nearZ; }
	float GetFarZ()		const { return m_farZ; }
	float GetFovY()		const { return m_fovY; }
	float GetFovX()		const { return m_fovX; }
	float GetAspect()	const { return m_aspect; }

	//�����ؾ���
	XMMATRIX View()				const { return XMLoadFloat4x4(&m_view); }
	XMMATRIX Projection()		const { return XMLoadFloat4x4(&m_proj); }
	XMMATRIX ViewProjection()	const { return XMLoadFloat4x4(&m_view) * XMLoadFloat4x4(&m_proj); }

	void SetResolutionFOV(UINT width, UINT height, float fov)
	{
		this->width = width;
		this->height = height;
		m_aspect = static_cast<float>(width) / static_cast<float>(height);
		m_fovX = fov * XM_PI / 180.0f;
		m_fovY = m_fovX * static_cast<float>(height) / static_cast<float>(width);
	}
	void SetNearZ(float nearZ)
	{
		m_nearZ = nearZ;
	}
	void SetFarZ(float farZ)
	{
		m_farZ = farZ;
	}

	void SetL(float v)
	{
		l = v;
	}

	int Sign(float v)
	{
		return v > 0 ? 1 : -1;
	}

	//ͨ��λ��+�۲���������ӽǾ���
	void LookAtXM(FXMVECTOR pos, FXMVECTOR lookAt, FXMVECTOR worldUp)
	{
		XMVECTOR look = XMVector3Normalize(lookAt - pos);
		XMVECTOR right = XMVector3Normalize(XMVector3Cross(worldUp, look));
		XMVECTOR up = XMVector3Cross(look, right);

		XMStoreFloat3(&m_position, pos);
		XMStoreFloat3(&m_right, right);
		XMStoreFloat3(&m_up, up);
		XMStoreFloat3(&m_look, look);

		r = XMVector3Length(pos).m128_f32[0];
		l = XMVector3Length(pos).m128_f32[0] * Sign(l); // change magnitude, keep sign
	}
	void LookAt(XMFLOAT3 pos, XMFLOAT3 lookAt, const XMFLOAT3 &worldUp)
	{
		XMVECTOR p = XMLoadFloat3(&pos);
		XMVECTOR l = XMLoadFloat3(&lookAt);
		XMVECTOR u = XMLoadFloat3(&worldUp);

		LookAtXM(p, l, u);
	}

	std::pair<XMVECTOR, bool> CrossSpace(XMVECTOR new_pos, XMVECTOR old_pos, Wormhole &wormhole)
	{
		//XMVECTOR offset = new_pos - old_pos;
		//XMVECTOR ref_offset = -old_pos;
		//float offset_length = XMVector3Length(offset).m128_f32[0];
		//float ref_length = XMVector3Length(ref_offset).m128_f32[0];
		//float dot = XMVector3Dot(ref_offset, offset).m128_f32[0];
		//float projected_offset_length = dot / ref_length;
		//return projected_offset_length > ref_length ? -1 : 1;
		float r = XMVector3Length(new_pos).m128_f32[0];
		if (r <= wormhole.radius)
		{
			float reflection_r = wormhole.radius - r;
			XMVECTOR reflection_dir = XMVector3Normalize(new_pos);
			XMVECTOR reflection_pos = new_pos + reflection_dir * reflection_r * 2.0f;
			return { reflection_pos, true };
		}
		else
			return { new_pos, false };
	}

	void NegDir(XMFLOAT3& dir)
	{
		XMVECTOR negDir = -XMLoadFloat3(&dir);
		XMStoreFloat3(&dir, negDir);
	}

	void SetPosition(XMFLOAT3 pos, Wormhole& wormhole)
	{
		XMVECTOR old_pos = XMLoadFloat3(&m_position);
		auto [new_pos, cross] = CrossSpace(XMLoadFloat3(&pos), old_pos, wormhole);
		l = (XMVector3Length(new_pos).m128_f32[0] - wormhole.radius) * Sign(l);
		if (cross)
		{
			l = -l;
			//pos = -new_pos;
			//NegDir(m_look);
			//NegDir(m_right);
			//NegDir(m_up);
		}

		r = XMVector3Length(new_pos).m128_f32[0];
		XMStoreFloat3(&m_position, new_pos);
	}

	//��������
	void Walk(float dt, Wormhole& wormhole)
	{
		dt *= Sign(l);
		XMVECTOR pos = XMLoadFloat3(&m_position);
		XMVECTOR look = XMLoadFloat3(&m_look);
		XMVECTOR old_pos = pos;
		pos += look * XMVectorReplicate(dt * m_speed);
		auto [new_pos, cross] = CrossSpace(pos, old_pos, wormhole);
		l = (XMVector3Length(new_pos).m128_f32[0] - wormhole.radius) * Sign(l);
		pos = new_pos;
		if (cross)
		{
			l = -l;
			//pos = -new_pos;
			//NegDir(m_look);
			//NegDir(m_right);
			//NegDir(m_up);
		}

		r = XMVector3Length(pos).m128_f32[0];
		XMStoreFloat3(&m_position, pos);
	}
	void Strafe(float dt, Wormhole& wormhole)
	{
		dt *= Sign(l);
		XMVECTOR pos = XMLoadFloat3(&m_position);
		XMVECTOR right = XMLoadFloat3(&m_right);
		XMVECTOR old_pos = pos;
		pos += right * XMVectorReplicate(dt * m_speed);
		auto [new_pos, cross] = CrossSpace(pos, old_pos, wormhole);
		l = (XMVector3Length(new_pos).m128_f32[0] - wormhole.radius) * Sign(l);
		pos = new_pos;
		if (cross)
		{
			l = -l;
			//pos = -new_pos;
			//NegDir(m_look);
			//NegDir(m_right);
			//NegDir(m_up);
		}

		r = XMVector3Length(pos).m128_f32[0];
		XMStoreFloat3(&m_position, pos);
	}
	void Fly(float dt, Wormhole& wormhole)
	{
		dt *= Sign(l);
		XMVECTOR pos = XMLoadFloat3(&m_position);
		XMVECTOR up = XMLoadFloat3(&m_up);
		XMVECTOR old_pos = pos;
		pos += up * XMVectorReplicate(dt * m_speed);
		auto [new_pos, cross] = CrossSpace(pos, old_pos, wormhole);
		l = (XMVector3Length(new_pos).m128_f32[0] - wormhole.radius) * Sign(l);
		pos = new_pos;
		if (cross)
		{
			l = -l;
			//pos = -new_pos;
			//NegDir(m_look);
			//NegDir(m_right);
			//NegDir(m_up);
		}

		r = XMVector3Length(pos).m128_f32[0];
		XMStoreFloat3(&m_position, pos);
	}
	void Pitch(float angle)
	{
		//angle *= Sign(l);
		XMMATRIX rotation = XMMatrixRotationAxis(XMLoadFloat3(&m_right), angle * m_rotateRate);

		XMStoreFloat3(&m_up, XMVector3TransformNormal(XMLoadFloat3(&m_up), rotation));
		XMStoreFloat3(&m_look, XMVector3TransformNormal(XMLoadFloat3(&m_look), rotation));
	}
	void RotateY(float angle)
	{
		//angle *= Sign(l);
		XMMATRIX rotation = XMMatrixRotationY(angle * m_rotateRate);

		XMStoreFloat3(&m_right, XMVector3TransformNormal(XMLoadFloat3(&m_right), rotation));
		XMStoreFloat3(&m_up, XMVector3TransformNormal(XMLoadFloat3(&m_up), rotation));
		XMStoreFloat3(&m_look, XMVector3TransformNormal(XMLoadFloat3(&m_look), rotation));
	}
	void Yaw(float angle)
	{
		//angle *= Sign(l);
		XMMATRIX rotation = XMMatrixRotationAxis(XMLoadFloat3(&m_up), angle * m_rotateRate);

		XMStoreFloat3(&m_right, XMVector3TransformNormal(XMLoadFloat3(&m_right), rotation));
		XMStoreFloat3(&m_look, XMVector3TransformNormal(XMLoadFloat3(&m_look), rotation));
	}
	void Roll(float angle)
	{
		//angle *= Sign(l);
		XMMATRIX rotation = XMMatrixRotationAxis(XMLoadFloat3(&m_look), angle * m_rotateRate);

		XMStoreFloat3(&m_right, XMVector3TransformNormal(XMLoadFloat3(&m_right), rotation));
		XMStoreFloat3(&m_up, XMVector3TransformNormal(XMLoadFloat3(&m_up), rotation));
	}

	//������ؾ���
	void UpdateView()
	{
		XMVECTOR r = XMLoadFloat3(&m_right);
		XMVECTOR u = XMLoadFloat3(&m_up);
		XMVECTOR l = XMLoadFloat3(&m_look);
		XMVECTOR p = XMLoadFloat3(&m_position);

		r = XMVector3Normalize(XMVector3Cross(u, l));
		u = XMVector3Normalize(XMVector3Cross(l, r));
		l = XMVector3Normalize(l);

		float x = -XMVectorGetX(XMVector3Dot(p, r));
		float y = -XMVectorGetX(XMVector3Dot(p, u));
		float z = -XMVectorGetX(XMVector3Dot(p, l));

		XMStoreFloat3(&m_right, r);
		XMStoreFloat3(&m_up, u);
		XMStoreFloat3(&m_look, l);
		XMStoreFloat3(&m_position, p);

		m_view(0, 0) = m_right.x;	m_view(0, 1) = m_up.x;	m_view(0, 2) = m_look.x;	m_view(0, 3) = 0;
		m_view(1, 0) = m_right.y;	m_view(1, 1) = m_up.y;	m_view(1, 2) = m_look.y;	m_view(1, 3) = 0;
		m_view(2, 0) = m_right.z;	m_view(2, 1) = m_up.z;	m_view(2, 2) = m_look.z;	m_view(2, 3) = 0;
		m_view(3, 0) = x;			m_view(3, 1) = y;		m_view(3, 2) = z;			m_view(3, 3) = 1;
	}
	void UpdateProj()
	{
		XMStoreFloat4x4(&m_proj, XMMatrixPerspectiveFovLH(m_fovY, m_aspect, m_nearZ, m_farZ));
	}

	auto GetWidth() const { return width; }
	auto GetHeight() const { return height; }
	auto GetL() const { return l; }
	auto GetR() const
	{
		return r;
	}

private:
	XMFLOAT3	m_right;			//λ�ü��������������
	XMFLOAT3	m_up;
	XMFLOAT3	m_look;
	XMFLOAT3	m_position;

	float		m_aspect;				//ͶӰ��ز���
	float		m_fovX;
	float		m_fovY;
	float		m_nearZ;
	float		m_farZ;

	XMFLOAT4X4	m_view;				//�ӽǾ���
	XMFLOAT4X4	m_proj;				//ͶӰ����

	float		m_speed;
	float		m_rotateRate;

	std::uint32_t width, height;

	float		l; // wormhole parameter
	float		r; // camera distance to center
};

