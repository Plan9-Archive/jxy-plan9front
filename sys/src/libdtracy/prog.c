#include <u.h>
#include <libc.h>
#include <dtracy.h>

int
dteverify(DTExpr *p)
{
	int i, nregs;
	u32int ins;
	u8int a, b, c;
	
	nregs = 16;
	for(i = 0; i < p->n; i++){
		ins = p->b[i];
		
		a = ins >> 16;
		b = ins >> 8;
		c = ins;
		switch(ins>>24){
		case DTE_ADD:
			if(ins == 0) continue;
			/* wet floor */
		case DTE_SUB:
		case DTE_MUL:
		case DTE_UDIV:
		case DTE_UMOD:
		case DTE_SDIV:
		case DTE_SMOD:
		case DTE_AND:
		case DTE_OR:
		case DTE_XOR:
		case DTE_XNOR:
		case DTE_LSL:
		case DTE_LSR:
		case DTE_ASR:
		case DTE_SEQ:
		case DTE_SNE:
		case DTE_SLT:
		case DTE_SLE:
			if(a >= nregs || b >= nregs || c >= nregs || c == 0)
				goto invalid;
			break;
		case DTE_LDI:
		case DTE_XORI:
			if(c >= nregs || c == 0)
				goto invalid;
			break;
		case DTE_BEQ:
		case DTE_BNE:
		case DTE_BLT:
		case DTE_BLE:
			if(a >= nregs || b >= nregs || i + 1 + c >= p->n)
				goto invalid;
			break;
		case DTE_RET:
			if(a >= nregs || b != 0 || c != 0)
				goto invalid;
			break;
		case DTE_LDV:
			if(a >= DTNVARS || b >= nregs)
				goto invalid;
			break;
		case DTE_ZXT:
		case DTE_SXT:
			if(a >= nregs || b == 0 || b > 64 || c >= nregs)
				goto invalid;
			break;
		default: goto invalid;
		}
	}
	if(p->n == 0 || p->b[p->n - 1] >> 24 != DTE_RET){
		werrstr("must end with RET");
		return -1;
	}
	return 0;

invalid:
	werrstr("invalid instruction %#.8ux @ %#.4ux", ins, i);
	return -1;
}

int
dtgverify(DTActGr *g)
{
	int i;

	if(g->pred != nil && dteverify(g->pred) < 0)
		return -1;
	for(i = 0; i < g->nact; i++)
		switch(g->acts[i].type){
		case ACTTRACE:
			if(g->acts[i].p == nil || dteverify(g->acts[i].p) < 0 || (uint)g->acts[i].size > 8)
				return -1;
			break;
		case ACTTRACESTR:
			if(g->acts[i].p == nil || dteverify(g->acts[i].p) < 0 || (uint)g->acts[i].size > DTRECMAX)
				return -1;
			break;
		default:
			return -1;
		}
	return 0;
}

typedef struct ExecInfo ExecInfo;
struct ExecInfo {
	int machno;
	int epid;
	u64int ts;
	u64int arg[10];
	DTChan *ch;
};

int
dteexec(DTExpr *p, ExecInfo *info, s64int *retv)
{
	s64int R[16];
	u32int ins;
	u8int a, b, c;
	int i;
	
	R[0] = 0;
	for(i = 0;; i++){
		ins = p->b[i];
		a = ins >> 16;
		b = ins >> 8;
		c = ins;
		switch(ins >> 24){
		case DTE_ADD: R[c] = R[a] + R[b]; break;
		case DTE_SUB: R[c] = R[a] - R[b]; break;
		case DTE_MUL: R[c] = R[a] * R[b]; break;
		case DTE_SDIV: if(R[b] == 0) goto div0; R[c] = R[a] / R[b]; break;
		case DTE_SMOD: if(R[b] == 0) goto div0; R[c] = R[a] % R[b]; break;
		case DTE_UDIV: if(R[b] == 0) goto div0; R[c] = (uvlong)R[a] / (uvlong)R[b]; break;
		case DTE_UMOD: if(R[b] == 0) goto div0; R[c] = (uvlong)R[a] % (uvlong)R[b]; break;
		case DTE_AND: R[c] = R[a] & R[b]; break;
		case DTE_OR: R[c] = R[a] | R[b]; break;
		case DTE_XOR: R[c] = R[a] ^ R[b]; break;
		case DTE_XNOR: R[c] = ~(R[a] ^ R[b]); break;
		case DTE_LDI: R[c] = (s64int)ins << 40 >> 54 << (ins >> 8 & 63); break;
		case DTE_XORI: R[c] |= (s64int)ins << 40 >> 54 << (ins >> 8 & 63); break;
		case DTE_LSL:
			if((u64int)R[b] >= 64)
				R[c] = 0;
			else
				R[c] = R[a] << R[b];
			break;
		case DTE_LSR:
			if((u64int)R[b] >= 64)
				R[c] = 0;
			else
				R[c] = (u64int)R[a] >> R[b];
			break;
		case DTE_ASR:
			if((u64int)R[b] >= 64)
				R[c] = R[a] >> 63;
			else
				R[c] = R[a] >> R[b];
			break;
		case DTE_SEQ: R[c] = R[a] == R[b]; break;
		case DTE_SNE: R[c] = R[a] != R[b]; break;
		case DTE_SLT: R[c] = R[a] < R[b]; break;
		case DTE_SLE: R[c] = R[a] <= R[b]; break;
		case DTE_BEQ: if(R[a] == R[b]) i += c; break;
		case DTE_BNE: if(R[a] != R[b]) i += c; break;
		case DTE_BLT: if(R[a] < R[b]) i += c; break;
		case DTE_BLE: if(R[a] <= R[b]) i += c; break;
		case DTE_LDV:
			switch(a){
			case DTV_ARG0:
			case DTV_ARG1:
			case DTV_ARG2:
			case DTV_ARG3:
			case DTV_ARG4:
			case DTV_ARG5:
			case DTV_ARG6:
			case DTV_ARG7:
			case DTV_ARG8:
			case DTV_ARG9:
				R[b] = info->arg[a - DTV_ARG0];
				break;
			case DTV_TIME: R[b] = info->ts; break;
			case DTV_MACHNO: R[b] = info->machno; break;
			default:
				R[b] = dtgetvar(a);
				break;
			}
		case DTE_ZXT: R[c] = (uvlong)R[a] << 64 - b >> 64 - b; break;
		case DTE_SXT: R[c] = (vlong)R[a] << 64 - b >> 64 - b; break;
		case DTE_RET: *retv = R[a]; return 0;
		}
	}

div0:
	snprint(info->ch->errstr, sizeof(info->ch->errstr), "division by zero");
	return -1;
}

int
dtpeekstr(uvlong addr, u8int *v, int len)
{
	int i;
	
	for(i = 0; i < len; i++){
		if(addr + i < addr || dtpeek(addr + i, &v[i], 1) < 0){
			memset(v, 0, len);
			return -1;
		}
		if(v[i] == 0)
			break;
	}
	if(i < len)
		memset(&v[i], 0, len - i);
	return 0;
}

#define PUT1(c) *bp++ = c;
#define PUT2(c) *bp++ = c; *bp++ = c >> 8;
#define PUT4(c) *bp++ = c; *bp++ = c >> 8; *bp++ = c >> 16; *bp++ = c >> 24;
#define PUT8(c) PUT4(c); PUT4(c>>32);

static int
dtgexec(DTActGr *g, ExecInfo *info)
{
	DTBuf *b;
	u8int *bp;
	s64int v;
	int i, j;
	
	b = g->chan->wrbufs[info->machno];
	if(b->wr + g->reclen > DTBUFSZ)
		return 0;
	if(g->pred != nil){
		if(dteexec(g->pred, info, &v) < 0)
			return -1;
		if(v == 0)
			return 0;
	}
	bp = &b->data[b->wr];
	PUT4(info->epid);
	PUT8(info->ts);
	for(i = 0; i < g->nact; i++){
		if(dteexec(g->acts[i].p, info, &v) < 0)
			return -1;
		switch(g->acts[i].type){
		case ACTTRACE:
			for(j = 0; j < g->acts[i].size; j++){
				*bp++ = v;
				v >>= 8;
			}
			break;
		case ACTTRACESTR:
			if(dtpeekstr(v, bp, g->acts[i].size) < 0){
				snprint(info->ch->errstr, sizeof(info->ch->errstr), "fault @ %#llux", v);
				return -1;
			}
			bp += g->acts[i].size;
			break;
		}
	}
	assert(bp - b->data - b->wr == g->reclen);
	b->wr = bp - b->data;
	return 0;
}

void
dtptrigger(DTProbe *p, int machno, uvlong arg0, uvlong arg1, uvlong arg2, uvlong arg3)
{
	DTEnab *e;
	ExecInfo info;
	
	info.ts = dttime();
	dtmachlock(machno);
	info.machno = machno;
	info.arg[0] = arg0;
	info.arg[1] = arg1;
	info.arg[2] = arg2;
	info.arg[3] = arg3;
	for(e = p->enablist.probnext; e != &p->enablist; e = e->probnext)
		if(e->gr->chan->state == DTCGO){
			info.ch = e->gr->chan;
			info.epid = e->epid;
			if(dtgexec(e->gr, &info) < 0)
				e->gr->chan->state = DTCFAULT;
		}
	dtmachunlock(machno);
}
